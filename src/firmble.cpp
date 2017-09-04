#include "firmble.h"

#include <iostream>
#include <sstream>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <cerrno>
#include <array>
#include <iomanip>
#include <vector>
#include <boost/optional.hpp>
#include <functional>

#include <signal.h>

#include <stdexcept>

#include <blepp/logging.h>
#include <blepp/pretty_printers.h>
#include <blepp/blestatemachine.h> //for UUID. FIXME mofo
#include <blepp/lescan.h>

#include <time.h>
#include <chrono>

using namespace std;
using namespace chrono;
using namespace BLEPP;

#define DBG(__x...) \
    if (s_debug) { \
        std::ostringstream __s; \
        struct timespec __t; \
        std::ios __f(NULL); \
        __f.copyfmt(__s); \
        clock_gettime(CLOCK_REALTIME, &__t); \
        __s << "DBG:" << syscall(SYS_gettid) << ":" \
            << __t.tv_sec << "." << setfill('0') << setw(9) << __t.tv_nsec << ":"; \
        __s.copyfmt(__f); \
        __s << __FILE__ ":" << __LINE__ << ":" << __FUNCTION__ << ":" \
            << __x << std::endl; \
        std::cerr <<__s.str(); \
    }

#define DBGPKT(__d, __l) \
    if (s_debug) { \
        std::ostringstream __s; \
        struct timespec __t; \
        std::ios __f(NULL); \
        __f.copyfmt(__s); \
        clock_gettime(CLOCK_REALTIME, &__t); \
        __s << "PKT:" << syscall(SYS_gettid) << ":" \
            << __t.tv_sec << "." << setfill('0') << setw(9) << __t.tv_nsec << ":"; \
        __s.copyfmt(__f); \
        __s << __FILE__ ":" << __LINE__ << ":" << __FUNCTION__ << ":" \
            << __l << ":" <<  hex << setw(2) ; \
        auto __p = __d; \
        size_t __n = __l; \
        while (__n > 0) { \
            __s << " " << (unsigned int)(*__p); \
            --__n; \
            ++__p; \
        } \
        __s << std::endl; \
        std::cerr <<__s.str(); \
    }

namespace {
    // debug integration
    int s_debug = 1;

    // time to shut down
    bool s_shutdown = false;

    // local methods

    // main thread waits for signal
    void wait_complete(int fd)
    {
        // wait for the event completion signal to be raised
        DBG("waiting for "<<fd);
        int result = -1;
        do
        {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(fd,&read_set);
            result = select(fd+1, &read_set, nullptr, nullptr, nullptr);
        } while (result < 0);
        uint64_t r;
        ::read(fd, &r, 8);
        DBG("completed on "<<fd<<" value "<<r);
    }

}

namespace firmata {

    // max number of queued messages
    size_t FirmBle::s_max_queued_messages = 1000;

    ///////////////////////////////////////////////////
    // Firmata methods

    // wrapper for starting thread
    void * FirmBle::thread_main(void * b)
    {
        DBG("entry "<<b);
        firmata::FirmBle * ble = reinterpret_cast<firmata::FirmBle*>(b);
        ble->ble_thread_main();
        DBG("exit "<<b);
        return nullptr;
    }

    FirmBle::FirmBle(const std::string &port)
    {
        DBG("this "<<this<<" port "<<port);
        // remember the device to connect to
        m_port = port;
        initialise();
    }
    FirmBle::FirmBle(int maxScanTime)
    {
        DBG("this "<<this<<" max scan time "<<maxScanTime);
        mScanMaxTime = maxScanTime;
        initialise();
    }
    void FirmBle::initialise()
    {
        // create sockets
        m_fdWakeBle    = eventfd(0, EFD_CLOEXEC|EFD_SEMAPHORE|EFD_NONBLOCK);
        m_fdWakeMain   = eventfd(0, EFD_CLOEXEC|EFD_SEMAPHORE|EFD_NONBLOCK);
        m_fdWakeMainRx = eventfd(0, EFD_CLOEXEC|EFD_SEMAPHORE|EFD_NONBLOCK);
        m_scancallback = std::bind(&firmata::FirmBle::ble_connect_scan_result,this);
        DBG("fds: ble "<<m_fdWakeBle<<" main "<<m_fdWakeMain << " rx "<<m_fdWakeMainRx);
        
        // create the worker thread
        pthread_create(&m_thread,
                       nullptr, 
                       &FirmBle::thread_main,
                       this);
        DBG("created thread");
    }

    FirmBle::~FirmBle()
    {
        DBG("entry "<<this);
        // disconnect if required
        close();

        // tell thread to stop and wait for it
        DBG("stopping thread");
        start_thread_and_wait(st_stop, true);
        DBG("stopped thread");
        pthread_join(m_thread, nullptr);
        DBG("joined thread");

        // tidy up
        ::close(m_fdWakeBle);
        ::close(m_fdWakeMain);
        ::close(m_fdWakeMainRx);
        DBG("done");
    }

    // firmata open connection
    // this triggers the ble connect
    void FirmBle::open()
    {
        // check for already connected
        if (m_connected) {
            DBG("already connected");
            throw firmata::IOException();
        }
        DBG("starting");
        start_thread_and_wait(st_conn);
        DBG("done");
    }

    // firmata check if connection is open
    bool FirmBle::isOpen()
    {
        return m_connected;
    }

    // firmata close
    // this triggers ble disconnect
    void FirmBle::close()
    {
        DBG("entry");
        start_thread_and_wait(st_disc, true);
        DBG("done");
    }

    // firmata queries how much data is queued
    size_t FirmBle::available()
    {
        DBG("entry");
        std::lock_guard<std::mutex> lock(m_rx_lock);
        DBG(m_rx_buf.size() << " entries");
        return m_rx_bytes;
    }

    // firmata read data
    std::vector<uint8_t> FirmBle::read(size_t size)
    {
        std::vector<uint8_t> bytes;
        DBG("size "<<size);

        pthread_yield();

        {
            std::lock_guard<std::mutex> lock(m_rx_lock);
            DBG(m_rx_buf.size() << " entries");

            if (!m_rx_buf.empty())
            {
                // concatenate all available packets
                while (!m_rx_buf.empty())
                {
                    BLEPP::PDUNotificationOrIndication & p = m_rx_buf.front();
                    const uint8_t* d = p.value().first;
                    size_t n = p.value().second - p.value().first;
                    if (n > size) {
                        DBG("no space left "<<n<<" space left "<<size);
                        break;
                    }
                    size-=n;
                    DBGPKT(d,n);
                    while (n > 0) {
                        bytes.push_back(*d);
                        --n;
                        ++d;
                    }
                    m_rx_buf.pop();
                }
                m_rx_bytes = 0;
            }
        }
        DBG("prepared result");

        pthread_yield();

        DBG("returning "<<bytes.size());
        return bytes;
    }

    // firmata transmit data in batches
    // call first with true then with false to release queued data
    void FirmBle::write_batch(bool start)
    {
        if (start) {
            m_writebatch = true;
        } else {
            m_writebatch = false;
            bool queued = false;
            {
                std::lock_guard<std::mutex> lock(m_tx_lock);
                if (!m_tx_queue.empty()) {
                    queued = true;
                }
            }
            // release any pending data
            if (queued) {
                DBG("waiting");
                start_thread_and_wait(st_write);
                pthread_yield();
                DBG("waited");
            }
        }
    }

    // firmata transmit data
    size_t FirmBle::write(std::vector<uint8_t> bytes)
    {
        // check for not connected
        if (!m_connected) {
            DBG("not connected");
            throw firmata::NotOpenException();
        }
        DBGPKT(bytes.begin(),bytes.size());
        {
            std::lock_guard<std::mutex> lock(m_tx_lock);
            m_tx_queue.push(bytes);
        }
        if (m_writebatch == false) {
            DBG("waiting");
            start_thread_and_wait(st_write);
            pthread_yield();
            DBG("waited");
        } else {
            DBG("batched mode");
        }
    }

    // firmata retrieve list of ports
    std::vector<BlePortInfo> FirmBle::listPorts(int maxTime /*seconds*/, int maxDevices)
    {
        firmata::FirmBle * ble = new firmata::FirmBle(maxTime);
        DBG("entry, ble "<<ble);

        // tell the object we want to do a scan
        ble->start_thread_and_wait(st_scan);
        DBG("waited, "<<ble->m_portList.size()<<" entries");

        // all done
        std::vector<BlePortInfo> ret = ble->m_portList;
        delete ble;
        DBG("done");
        return ret;
    }

    //////////////////////////////////////////////////////////////
    //
    // response handlers

    // scan results handler
    void FirmBle::ble_handle_scan_result()
    {
        DBG("entry "<<this);
        vector<AdvertisingResponse> ads = m_scanner->get_advertisements();

        for(const auto& ad: ads)
        {
            // if device under consideration is connectable
            DBG("device "<<ad.address);
            if((ad.type == LeAdvertisingEventType::ADV_IND) ||
               (ad.type == LeAdvertisingEventType::ADV_DIRECT_IND))
            {
                DBG("connectable");
                for(const auto& uuid: ad.UUIDs)
                {
                    DBG("    uuid: "<<to_str(uuid));
                    // and if it is advertising the firmata service
                    if (to_str(uuid) == "6e400001-b5a3-f393-e0a9-e50e24dcca9e")
                    {
                        // we have a candidate
                        m_portList.push_back(BlePortInfo({
                            ad.address, (ad.local_name)?"":ad.local_name->name, ""
                        }));
                        break;
                    }
                }
            } else {
                DBG("not connectable");
            }
        }
        DBG("valid devices: "<<m_portList.size());

        // if port is empty then we are connecting to the first located device
        if ((m_job != st_scan) && (m_port == ""))
        {
            if (m_portList.empty())
            {
                // normal client request, all done
                m_result = "No suitable devices found";
                wakeMain();
                DBG("done");
            }
            else
            {
                // so we must remember the MAC
                m_port = m_portList[0].port;
                DBG("autoselected port "<<m_port);
                // initiate the connect by finding the descriptors
                m_gatt->connect_nonblocking(m_port);
                DBG("started connect");
            }
        }
        else
        {
            // normal client request, all done
            //DBG("done");
            //wakeMain();
            // wait for timeout to complete scan
        }
    }

    // check remote characteristics during connect
    void FirmBle::ble_connect_scan_result ()
    {
        DBG("entry");
        //pretty_print_tree(m_gatt);

        // look thru reported services + characteristics to find
        // firmata characteristics
        for(auto& service: m_gatt->primary_services)
        {
            DBG("service");
            for(auto& characteristic: service.characteristics)
            {
                DBG(" characteristic: "<<to_str(characteristic.uuid));
                if(characteristic.uuid == UUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e"))
                {
                    // for sending requests
                    //cout << "got tx" << endl;
                    m_tx = new Characteristic(characteristic);
                    DBG("    found tx");
                }

                if(characteristic.uuid == UUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e"))
                {
                    // for receiving responses
                    //cout << "got rx" << endl;
                    m_rx = new Characteristic(characteristic);
                    DBG("    found rx");
                }
            }
        }
        if (m_tx == nullptr) {
            m_result = "Failed to find TX characteristic";
            wakeMain();
        } else if (m_rx == nullptr) {
            m_result = "Failed to find RX characteristic";
            wakeMain();
        } else {
            // initiate the connect
            DBG("initiating connect");
            m_rx->cb_notify_or_indicate = std::bind(&firmata::FirmBle::ble_rx,this, std::placeholders::_1);
            m_gatt->cb_notify_or_indicate = std::bind(&firmata::FirmBle::ble_rx2,this, std::placeholders::_1, std::placeholders::_2);
            try
            {
                ble_connected();
            }
            catch (...)
            {
                DBG("caught");
                m_result = "Cannot connect";
                wakeMain();
            }
        }
        DBG("done");
    };

    // device connected
    void FirmBle::ble_connected ()
    {
        DBG("entry");
        // enable notify
        m_rx->set_notify_and_indicate(true, false);
        DBG("done set_notify");
        // then set reporting interval to something a little slower
        // than is normally used on a serial link
        m_gatt->cb_write_response = std::bind(&firmata::FirmBle::ble_connect_reporting_set,this);
        static unsigned char set_reporting_interval[] = { 0xf0, 0x7a, 0, 8, 0xf7 };
        try
        {
            DBG("sending set reporting interval");
            m_tx->write_request(set_reporting_interval, 5);
        }
        catch (...)
        {
            DBG("caught");
            m_result = "Failed to set reporting interval";
            wakeMain();
        }
        DBG("done");
    }

    // device reporting interval is now set up
    void FirmBle::ble_connect_reporting_set ()
    {
        DBG("entry");
        // all done
        // reinstate the write done handler
        m_gatt->cb_write_response = std::bind(&firmata::FirmBle::ble_write_done,this);
        m_result = "";
        // wake the caller
        m_connected = true;
        wakeMain();
        DBG("done");
    }

    // device disconnected
    // could happen any time
    void FirmBle::ble_disconnected (BLEPP::BLEGATTStateMachine::Disconnect d)
    {
        DBG("entry "<<BLEPP::BLEGATTStateMachine::get_disconnect_string(d)<< " job "<<m_job);
        m_connected = false;
        if (m_job == st_disc)
        {
            // disconnect is expected
            m_result = "";
            wakeMain();
            DBG("done");
        } else {
            m_result = "Disconnected";
            if (m_job != st_idle) {
                // abandon anything else in progress
                // TODO should we auto reconnect?
                wakeMain();
                // wake a reader using other fd
                wakeRx();
                DBG("done");
            }
        }
    }

    // all data written
    void FirmBle::ble_write_done ()
    {
        DBG("entry, clearing buffer "<<reinterpret_cast<void*>(m_tx_buff));
        if (m_tx_buff)
        {
            free(m_tx_buff);
            m_tx_buff = nullptr;
            wakeMain();
        }
        DBG("done");
    }

    // data received from device
    void FirmBle::ble_rx(const PDUNotificationOrIndication & p) {
        size_t n = p.value().second - p.value().first;
        DBG("entry "<<n<< " bytes");
        DBGPKT(p.value().first,p.value().second - p.value().first);
        {
            // receive data
            std::lock_guard<std::mutex> lock(m_rx_lock);
            // limit queue length
            if (m_rx_buf.size() > s_max_queued_messages) {
                // too many queued messages, drop the oldest
                BLEPP::PDUNotificationOrIndication & p2 = m_rx_buf.front();
                m_rx_bytes -= p2.value().second - p2.value().first;
                m_rx_buf.pop();
            }
            m_rx_buf.push(p);
            m_rx_bytes += n;
            DBG("queue now "<<m_rx_buf.size()<<" entries, "<<m_rx_bytes<<" bytes");
        }
        wakeRx();
        DBG("done");
    }

    void FirmBle::ble_rx2(const Characteristic &, const PDUNotificationOrIndication & p) {
        ble_rx(p);
    }


    //////////////////////////////////////////////////////////////
    //
    // Thread methods

    // main thread uses this to wake ble thread and wait for completion
    void FirmBle::start_thread_and_wait(ble_job job, bool nothrow)
    {
        DBG("entry, job "<<job<<" nothrow "<<nothrow);
        // record what to do
        m_job = job;
        // prepare the semaphore value
        uint64_t r = 1;
        // wake the caller
        ::write(m_fdWakeBle, reinterpret_cast<const void *>(&r), 8);
        // wait for tx ack
        DBG("waiting");
        wait_complete(m_fdWakeMain);
        DBG("done");
        if ((nothrow == false) && (m_result.size())) {
            DBG("error "<<m_result);
            throw firmata::IOException();
        }
    }

    // ble thread flags as idle
    void FirmBle::jobDone()
    {
        // mark ourselves as inactive
        m_job = st_idle;
        m_active = false;
    }

    // ble thread completes a job and wakes something waiting for receive
    void FirmBle::wakeRx()
    {
        uint64_t r = 0;
        DBG("entry");
        {
            std::lock_guard<std::mutex> lock(m_rx_lock);
            DBG("waiting "<<m_rx_reader_waiting);
            if (m_rx_reader_waiting)
            {
                // prepare the return value
                r = 1;
                m_rx_reader_waiting = false;
            }
        }

        if (r != 0)
        {
            DBG("signalling main");
            jobDone();
            // wake the caller
            ::write(m_fdWakeMainRx, reinterpret_cast<const void *>(&r), 8);
            DBG("done");
        }
    }
    // ble thread completes a job and wakes main
    void FirmBle::wakeMain()
    {
        DBG("entry");
        // prepare the return value
        uint64_t r = 1;
        jobDone();
        // wake the caller
        ::write(m_fdWakeMain, reinterpret_cast<const void *>(&r), 8);
        DBG("done");
    }

    // ble thread write helper
    void FirmBle::do_write()
    {
        DBG("entry");
        int i=0;
        {
            size_t bufsiz = 256;
            m_tx_buff = reinterpret_cast<uint8_t *>(malloc(bufsiz));
            std::lock_guard<std::mutex> lock(m_tx_lock);
            DBG(m_tx_queue.size()<<" entries to send");
            while (!m_tx_queue.empty())
            {
                std::vector<uint8_t> & r = m_tx_queue.front();
                DBG(r.size()<<" bytes in buffer");
                for(auto& b: r)
                {
                    m_tx_buff[i++]=b;
                    if (i > bufsiz)
                    {
                        DBG("reallocating at bufsiz");
                        bufsiz *= 2;
                        m_tx_buff = reinterpret_cast<uint8_t *>(realloc(m_tx_buff, bufsiz));
                    }
                }
                DBG("now have "<<i<<" bytes");
                m_tx_queue.pop();
            }
        }
        DBG("sending "<<i<<" bytes from "<<reinterpret_cast<void*>(m_tx_buff));
        DBGPKT(m_tx_buff,i);
        m_tx->write_request(m_tx_buff, i);
    }

    // ble thread
    void FirmBle::ble_thread_main()
    {
        DBG("entry");

        fd_set write_set, read_set;
        bool finished = false;
        while (finished == false)
        {
            struct timeval tv;
            int result;
            do
            {
                FD_ZERO(&read_set);
                FD_ZERO(&write_set);
                int fd_max = -1;

                if (m_gatt) {
                    if (m_gatt->socket() != -1)
                    {
                        //Reads are always a possibility due to asynchronus notifications.
                        DBG("adding gatt socket "<<m_gatt->socket()<<" to read");
                        FD_SET(m_gatt->socket(), &read_set);
                        if (m_gatt->socket() > fd_max) {
                            fd_max = m_gatt->socket();
                        }

                        //Writes are usually available, so only check for them when the 
                        //state machine wants to write.
                        if(m_gatt->wait_on_write()) {
                            DBG("adding gatt socket "<<m_gatt->socket()<<" to write");
                            FD_SET(m_gatt->socket(), &write_set);
                        }
                    } else {
                        DBG("no gatt socket");
                    }
                }

                if (!m_active) {
                    // add event fd only if idle
                    DBG("adding main fd "<<m_fdWakeBle);
                    FD_SET(m_fdWakeBle, &read_set);
                    if (m_fdWakeBle > fd_max) { fd_max = m_fdWakeBle; }
                }

                if (m_scanner != nullptr) {
                    // add scanner fd if required
                    DBG("adding scanner fd "<<m_scanner->get_fd());
                    FD_SET(m_scanner->get_fd(), &read_set);
                    if (m_scanner->get_fd() > fd_max) { fd_max = m_scanner->get_fd(); }
                }

                // use the timer to limit scan completion
                if (m_job == st_scan)
                {
                    // delay time was set up at scan initiation
                } else {
                    // wait for max 10 sec
                    tv.tv_sec = 10;
                    tv.tv_usec = 0;
                }
                result = select(fd_max+1, &read_set, &write_set, nullptr, &tv);
                if (result >= 0)
                {
                    break;
                }
                DBG("trying select again after "<<errno);
            } while(true);

            // timeout
            if (result == 0) {
                if (m_job == st_scan) {
                    // scan completed
                    DBG("scan timed out");
                    //ble_handle_scan_result();
                    wakeMain();
                }
                continue;
            }

            if (m_gatt != nullptr) {
                if (m_gatt->socket() != -1) {
                    if(FD_ISSET(m_gatt->socket(), &write_set)) {
                        DBG("gatt write");
                        m_gatt->write_and_process_next();
                    }

                    if(FD_ISSET(m_gatt->socket(), &read_set)) {
                        DBG("gatt read");
                        m_gatt->read_and_process_next();
                    }
                } else {
                    DBG("no gatt socket");
                }
            }

            if((m_scanner != nullptr) && (FD_ISSET(m_scanner->get_fd(), &read_set))) {
                DBG("scanner");
                ble_handle_scan_result();
            }

            if(FD_ISSET(m_fdWakeBle, &read_set))
            {
                DBG("wake, job "<<m_job);
                // flush the socket
                wait_complete(m_fdWakeBle);
                // determine what to do
                switch (m_job) {
                    case st_idle: // idle
                        // err?
                        break;

                    case st_scan: // initiate scan
                        try
                        {
                            m_scanner = new BLEPP::HCIScanner(true);
                            m_active = true;
                            // this will get counted down by successive select calls
                            tv.tv_sec = mScanMaxTime;
                            tv.tv_usec = 0;
                        }
                        catch (...)
                        {
                            DBG("caught");
                            m_result = "scan failed";
                            wakeMain();
                        }
                        break;

                    case st_conn: // initiate connect
                        try
                        {
                            if (m_gatt == nullptr)
                            {
                                // create new GATT object
                                m_gatt = new BLEPP::BLEGATTStateMachine;
                                m_gatt->cb_disconnected = std::bind(&firmata::FirmBle::ble_disconnected,this,std::placeholders::_1);
                                m_gatt->cb_connected = std::bind(&firmata::FirmBle::ble_connected,this);
                                m_gatt->setup_standard_scan(m_scancallback);
                                DBG("m_gatt "<<m_gatt);
                            }
                            if (m_port == "")
                            {
                                // if no port specified, do a scan to find
                                // the first device
                                m_scanner = new BLEPP::HCIScanner(true);
                                DBG("m_scanner "<<m_scanner);
                            } else {
                                // initiate the connect by finding the descriptors
                                DBG("connect");
                                m_gatt->connect_nonblocking(m_port);
                            }
                            m_active = true;
                            DBG("done");
                        }
                        catch (...)
                        {
                            DBG("caught");
                            // any error -> disconnect
                            if (m_gatt)
                            {
                                try
                                {
                                    DBG("closing");
                                    m_gatt->close();
                                }
                                catch (...)
                                {
                                    DBG("caught");
                                }
                                m_active = false;
                            }
                            wakeMain();
                        }
                        DBG("finished");
                        break;

                    case st_disc: // initiate disconnect
                        // stop any scanner that is running
                        if (m_scanner)
                        {
                            try
                            {
                                m_scanner->stop();
                                DBG("stopped scanner");
                            }
                            catch (...)
                            {
                                DBG("caught");
                            }
                            delete m_scanner;
                            m_scanner = nullptr;
                        }

                        if (m_gatt)
                        {
                            try
                            {
                                // TODO send reset or at least stop events
                                DBG("closing");
                                if (m_gatt != nullptr)
                                {
                                    m_gatt->close();
                                }
                            }
                            catch (...)
                            {
                                DBG("caught");
                            }
                        }
                        // disconnect appears instant
                        if ((m_scanner == nullptr) && (m_gatt == nullptr || m_gatt->socket() == -1))
                        {
                            DBG("connection is now inactive");
                            m_active = false;
                            wakeMain();
                        }
                        DBG("done");
                        break;

                    case st_write: // send data
                        try
                        {
                            do_write();
                            m_active = true;
                        }
                        catch (...)
                        {
                            DBG("caught");
                            wakeMain();
                        }
                        DBG("done");
                        break;

                    case st_stop: // all done
                        m_active = false;
                        if (m_scanner)
                        {
                            try
                            {
                                m_scanner->stop();
                                DBG("stopped scanner");
                            }
                            catch (...)
                            {
                                DBG("caught");
                            }
                            delete m_scanner;
                            m_scanner = nullptr;
                        }
                        if (m_gatt)
                        {
                            try
                            {
                                m_gatt->close();
                                DBG("closed gatt");
                            }
                            catch (...)
                            {
                                DBG("caught");
                            }
                            delete m_gatt;
                            m_gatt = nullptr;
                        }
                        if (m_tx_buff)
                        {
                            free(m_tx_buff);
                            m_tx_buff = nullptr;
                        }
                        finished = true;
                        DBG("done");
                        wakeMain();
                        break;
                }
            }

        } // end of main loop
    } // end of thread main
} // end namespace



#if 0
//cout << "state "<<state<<endl;
switch (state) {
    case 0: // reading services
        if (m_gatt.is_idle()) {
            cout << endl;
            ++state;
        } else {
            break;
        }

    case 1: // services read
        if (rx == nullptr) {
            cerr << "no receive service found" << endl;
            state = 7;
        } else {
            ++state;
            cout << "setup rx" << endl;
            rx->cb_notify_or_indicate = notify_cb;
            m_gatt.cb_notify_or_indicate = notify_cb2;
            rx->set_notify_and_indicate(true, false);
        }
        break;

    case 2: // setting notify
        if (m_gatt.is_idle()) {
            cout << endl;
            ++state;
        } else {
            break;
        }

    case 3: // set reporting interval
        {
            unsigned char enable[] = { 0xf0, 0x7a, 0, 8, 0xf7 };
            tx->write_request(enable, 5);
        }
        ++state;
        break;

    case 4: // setting reporting interval
        if (m_gatt.is_idle()) {
            cout << endl;
            ++state;
        } else {
            break;
        }

    case 5: // notify set
        {
            unsigned char enable[2] = { 0xc0, 0x01 };
            tx->write_request(enable, 2);
        }
        ++state;
        break;

    case 6: // enabling reporting
        if (m_gatt.is_idle()) {
            cout << endl;
            ++state;
        } else {
            break;
        }

    case 7: // reporting enabled
        if (tx == nullptr) {
            cerr << "no tx characteristic found" << endl;
            exit(1);
        }
        cout << endl;
        ++state;
}
#endif
