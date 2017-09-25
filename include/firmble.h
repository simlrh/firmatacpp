#ifndef		__FIRMBLE_H_
#define		__FIRMBLE_H_

#include <queue>
#include <mutex>
#include "firmio.h"
#include "blepp/blestatemachine.h"
#include "blepp/lescan.h"

namespace firmata {

	typedef struct BlePortInfo {
		std::string port;
		std::string description;
		std::string hardware_id;
	} BlePortInfo;

	class FirmBle : public FirmIO {
	public:
		FirmBle(const std::string &port = ""); /* normal use */
		FirmBle(int maxScanTime); /* used for initiating a scan */
		virtual ~FirmBle();

                /////////////////////////////////////////////////////////////
                // Firmata methods

                // firmata open connection
                // this triggers the ble connect
		virtual void open() override;
                // firmata check if connection is open
		virtual bool isOpen() override;
                // firmata close
                // this triggers ble disconnect
		virtual void close() override;
                // firmata queries how much data is queued
		virtual size_t available() override;
                // firmata read data
		virtual std::vector<uint8_t> read(size_t size = 1) override;
                // firmata transmit data
		virtual size_t write(std::vector<uint8_t> bytes) override;
                // firmata transmit data in batches
                // call first with true then with false to release queued data
		void write_batch(bool start);
                // firmata retrieve list of ports
		static std::vector<BlePortInfo> listPorts(int timeout=10 /*seconds*/, int maxDevices=0);
                // enable ble debug
                static void enableDebug(bool enable=true);

	private:

                /////////////////////////////////////////////////////////////
                // members etc.

                // state 
                enum ble_job {
                    st_idle, // idle
                    st_scan, // running scan
                    st_conn, // connecting
                    st_disc, // disconnecting
                    st_write, // send data
                    st_stop,  // shutdown
                };
                ble_job m_job = st_idle;

                // do we think we are connected?
                bool m_connected = false;

                // are we busy doing something
                bool m_active = false;

                // file descriptors for waking each side
                int m_fdWakeBle = -1;
                int m_fdWakeMain = -1;
                int m_fdWakeMainRx = -1;

                // memory buffer for sending data from
                uint8_t * m_tx_buff = nullptr;
                // sets of bytes to be sent
                std::queue< std::vector<uint8_t> > m_tx_queue;
                // access lock to transmit queue
                std::mutex m_tx_lock;

                // received PDUs
                // stored like this to make it easy to limit queue length
                // without overwriting partial messages
                std::queue<BLEPP::PDUNotificationOrIndication> m_rx_buf;
                // easier to count bytes here than calculate on demand
                volatile size_t m_rx_bytes = 0;
                // max number of messages in receive queue
                static size_t s_max_queued_messages;
                // access lock to receive queue
                std::mutex m_rx_lock;
                // is anyone waiting to read from the rx queue?
                volatile bool m_rx_reader_waiting = false;

                // BLE handles
                BLEPP::BLEGATTStateMachine * m_gatt = nullptr;
                BLEPP::Characteristic * m_tx = nullptr;
                BLEPP::Characteristic * m_rx = nullptr;

                // scan stuff
                BLEPP::HCIScanner * m_scanner = nullptr;
                int mScanMaxTime = 10;
                // list of "ports" (remote devices) that we will report
                // to main
                std::vector<BlePortInfo> m_portList;

                // worker thread handle
                pthread_t m_thread;
                
                // results - empty for OK, message otherwise
                std::string m_result;

                // the port we have been asked to communicate with
                std::string m_port;

                // connect callback
                std::function<void()> m_scancallback;

                // are we batching writes
                bool m_writebatch = false;

                /////////////////////////////////////////////////////////////
                // ble methods
                // called in context of worker thread

                // common constructor path
                void initialise();

                // scan results handler
                void ble_handle_scan_result();
                // check remote characteristics during connect
                void ble_connect_scan_result ();
                // device connected
                void ble_connected ();
                // device reporting interval is now set up
                void ble_connect_reporting_set ();
                // device disconnected
                // could happen any time
                void ble_disconnected (BLEPP::BLEGATTStateMachine::Disconnect);
                // all data written
                void ble_write_done ();
                // data received from device
                void ble_rx(const BLEPP::PDUNotificationOrIndication & p);
                void ble_rx2(const BLEPP::Characteristic &, const BLEPP::PDUNotificationOrIndication & p);

                /////////////////////////////////////////////////////////////
                // threading methods

                // main thread uses this to wake ble thread and wait for completion
                void start_thread_and_wait(ble_job job, bool nothrow = false);
                // ble thread sets flags as idle
                void jobDone();
                // ble thread completes a job and wakes something waiting for receive
                void wakeRx();
                // ble thread completes a job and wakes main
                void wakeMain();
                // ble thread write helper
                void do_write();
                // ble thread
                static void * thread_main(void * b);
                void ble_thread_main();
	};

}

#endif

