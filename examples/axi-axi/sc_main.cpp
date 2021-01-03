/*
 * sc_main.cpp
 *
 *  Created on:
 *      Author: Rocco Jonack
 *
 *  Description: Simple AXI example showing an initiator and a target
 *               communicating using an AXI socket based on TLM2 + AXI 
 *               specific extensions
 *               The communication shows the basic transition states for
 *               requests and responses, but focuses on simple transactions
 *               in order to avoid dependencies
 *               The only dependencies are SystemC, TLM2 and AXI extension libraries
 *               There are several known limitations in the example
 *               - no out of order behavior
 *               - only one outstanding transfer
 *               - no memory management for the payload
 *               - no data transfer
 *               - no configurability without touching the code
 *               - very verbose execution (implying slow execution)
 */

#include <axi_tlm.h>
// using namespace axi;
using namespace sc_core;
using namespace std;

const unsigned SOCKET_WIDTH = 64;
const unsigned wData = SOCKET_WIDTH/8;
const unsigned AXI_SIZE = 5;
const unsigned Max_Length = 64;
const unsigned int NumberOfIterations = 10;
const unsigned int ClockPeriodNS = 10;
unsigned int addr = 0;
sc_core::sc_time delay_time = SC_ZERO_TIME;
  
typedef  axi::axi_protocol_types::tlm_payload_type payload_type;
typedef  axi::axi_protocol_types::tlm_phase_type   phase_type;

bool prepare_trans(tlm::tlm_generic_payload* trans, unsigned int iteration) {
  axi::axi4_extension* ext;
  unsigned int len = ((iteration)*wData)%Max_Length;
  if (len==0) len =Max_Length;
  if (trans) {
    ext = trans->get_extension<axi::axi4_extension>();
  }
  else {
    cout << sc_time_stamp() << " no transaction pointer?" << endl;
    return false;
  }
  if (!ext) {
    ext = new axi::axi4_extension();
    trans->set_extension<axi::axi4_extension>(ext);
  }
  trans->set_data_length(len);
  trans->set_streaming_width(len);
  if (iteration%2)
    trans->set_command(tlm::TLM_READ_COMMAND);
  else
    trans->set_command(tlm::TLM_WRITE_COMMAND);
  trans->set_address(addr);
  addr += len;
  trans->set_data_ptr(NULL);
  if (ext) {
    ext->set_size(AXI_SIZE);
    ext->set_id(axi::common::id_type::CTRL, 0);
    ext->set_length(len/wData);
    ext->set_burst(axi::burst_e::INCR);
  }
  else {
    cout << sc_time_stamp() << " no extension pointer?" << endl;
    return false;
  };
  return true;
}

class MyMaster : public sc_module, axi::axi_bw_transport_if<axi::axi_protocol_types> {	
public : 
   axi::axi_initiator_socket<SOCKET_WIDTH> isocket{"isocket"};
   sc_in< bool > Rst{"Rst"};
   sc_in< bool > Clk{"Clk"};  
   
   MyMaster(sc_module_name mod) : sc_module(mod) {	
     SC_THREAD(requestSender); 
     sensitive << Clk.pos();
     trans = new tlm::tlm_generic_payload();
     isocket(*this);
   }
   virtual ~MyMaster() {};
private :
   SC_HAS_PROCESS(MyMaster);
   tlm::tlm_generic_payload* trans{NULL};
   sc_event responseReceived;

   void b_snoop(payload_type& trans, sc_core::sc_time& t) {
     cout << sc_time_stamp() << " " <<name()<< " b_snoop not implemented" << endl;
   };

   tlm::tlm_sync_enum nb_transport_bw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) {
     cout << sc_time_stamp() << " " <<name() << " nb_transport_bw received with phase " << phase << endl;
     tlm::tlm_sync_enum  return_status = tlm::TLM_ACCEPTED;
     if (phase==axi::BEGIN_PARTIAL_RESP) {
       responseReceived.notify(ClockPeriodNS/2,SC_NS);    
       phase = axi::END_PARTIAL_RESP;   
       isocket->nb_transport_fw(trans, phase, delay_time);
       cout << sc_time_stamp() << " " << name()<< " nb_transport_bw sending END_PARTIAL_RESP" << endl;
       return return_status;
     };
     if (phase==axi::END_PARTIAL_REQ) {
       return return_status;
     };
     if (phase==tlm::BEGIN_RESP) {
       responseReceived.notify(ClockPeriodNS/2,SC_NS);    
       phase = tlm::END_RESP;   
       isocket->nb_transport_fw(trans, phase, delay_time);
       cout << sc_time_stamp() << " " << name()<< " nb_transport_bw sending END_RESP" << endl;
       return tlm::TLM_COMPLETED;
     }; 
     if (phase==tlm::END_REQ) {
       return return_status;
     };
     cout << sc_time_stamp() << " " << name()<< " nb_transport_bw illegal phase " << phase << endl;
     return return_status; 
   };
   
   void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {  
     cout << sc_time_stamp() << " " <<name() << " invalidate_direct_mem_ptr not implemented" << endl;
   };
     
   void requestSender() {
     while(Rst.read()) wait();
     cout << sc_time_stamp() << " " << name() << " out of reset" << endl;
     wait(ClockPeriodNS, SC_NS);
     for(auto i=1; i<NumberOfIterations; ++i) {
       cout << sc_time_stamp() << " " << name() << " executing transactions in iteration " << i << endl;
       bool success = prepare_trans(trans, i);
       if (!success) {
	 cout << sc_time_stamp() << " " << name() << " somthing went wrong in transaction preparation of iteration " << i << endl;
	 sc_stop();
       };
       if (trans->get_command()==tlm::TLM_WRITE_COMMAND) {
	 for (auto j=1; j<trans->get_data_length()/wData; ++j) {
	   cout << sc_time_stamp() << " " << name() << " sending write request " << j << endl;
	   tlm::tlm_phase  phase = axi::BEGIN_PARTIAL_REQ;
	   isocket->nb_transport_fw(*trans, phase, delay_time);
	   wait();
	 }
	 cout << sc_time_stamp() << " " << name() << " sending last write request " << endl;
	 tlm::tlm_phase  phase = tlm::BEGIN_REQ;
	 isocket->nb_transport_fw(*trans, phase, delay_time);
       }
       else {
	 cout << sc_time_stamp() << " " << name() << " sending read request" <<endl;
	 tlm::tlm_phase  phase = tlm::BEGIN_REQ;
	 isocket->nb_transport_fw(*trans, phase,delay_time);
       }
       if (trans->get_command()==tlm::TLM_WRITE_COMMAND) {
	 cout << sc_time_stamp() << " " << name() << " waiting for write response " << endl;
	 wait(responseReceived);
	 cout << sc_time_stamp() << " " << name() << " got write response:"  <<endl;
       }
       else {
	 for (auto j=0; j<trans->get_data_length()/wData; ++j) {
	   cout << sc_time_stamp() << " " << name() << " waiting for read response " << j << endl;
	   wait(responseReceived);
	   cout << sc_time_stamp() << " " << name() << " got read response:" <<endl;
	 }
       }
       cout << sc_time_stamp() << " " << name() << " finished transaction for iteration " << i << endl;
       wait(10*ClockPeriodNS, SC_NS);
     }
     wait(10*ClockPeriodNS, SC_NS);
     cout << sc_time_stamp() << " " << name() << " finished workload for " << NumberOfIterations << " iterations" << endl;
     sc_stop();
   };
};

class MySlave : public sc_module, axi::axi_fw_transport_if<axi::axi_protocol_types> {
public :
  axi::axi_target_socket<SOCKET_WIDTH> tsocket{"tsocket"};
  sc_in< bool > Rst{"Rst"};
  sc_in< bool > Clk{"Clk"};

  MySlave(sc_module_name name_) : sc_module(name_) {
    SC_THREAD(handleResponses);
    sensitive << Clk.pos();
    tsocket(*this);
  }
  virtual ~MySlave() {};
private :
  SC_HAS_PROCESS(MySlave);
  sc_event requestReceived;
  sc_event responseEndReceived;
  tlm::tlm_generic_payload* tr;
  
  void b_transport(payload_type& trans, sc_core::sc_time& t) {
    cout << sc_time_stamp() << " " <<name() << " b_transport not implemented" << endl;
  };
  
  bool get_direct_mem_ptr(payload_type& trans, tlm::tlm_dmi& dmi_data) {
    cout << sc_time_stamp() << " " <<name() << " get_direct_mem_ptr not implemented" << endl;
    return false;
  };
  
  unsigned int transport_dbg(payload_type& trans) {
    cout << sc_time_stamp() << " " <<name() << " transport_dbg not implemented" << endl;
    return 0;
  };

  tlm::tlm_sync_enum nb_transport_fw
  (   payload_type&      gp            
    , phase_type&        phase         
    , sc_core::sc_time&  delay_time)   
  {
    cout << sc_time_stamp() << " " << name() << " nb_transport_fw received phase " << phase << endl;
    tlm::tlm_sync_enum  return_status = tlm::TLM_COMPLETED;
    if(phase==axi::BEGIN_PARTIAL_REQ) {
      tr = &gp; 
      phase = axi::END_PARTIAL_REQ;   
      return_status = tlm::TLM_UPDATED; 
      tsocket->nb_transport_bw(gp, phase, delay_time);
      cout << sc_time_stamp() << " " << name()<< " nb_transport_fw sending END_PARTIAL_REQ" << endl;
      return return_status;  
    };
    if(phase==tlm::BEGIN_REQ) {
      tr = &gp; 
      requestReceived.notify();  
      phase = tlm::END_REQ;   
      return_status = tlm::TLM_UPDATED; 
      tsocket->nb_transport_bw(gp, phase, delay_time);
      cout << sc_time_stamp() << " " << name()<< " nb_transport_fw sending END_REQ" << endl;
      return return_status;  
    } 
    if(phase==tlm::END_RESP) {
      responseEndReceived.notify(ClockPeriodNS/2,SC_NS);    
      return return_status;  
    }
    if(phase==axi::END_PARTIAL_RESP) {
      responseEndReceived.notify(ClockPeriodNS/2,SC_NS);    
      return return_status;  
    }
    cout << sc_time_stamp() << " " << name()<< " nb_transport_fw illegal phase " << phase << endl;
    return tlm::TLM_ACCEPTED;
  };

  void handleResponses() {
    phase_type        phase ;	
    while(Rst.read()) wait();
    cout << sc_time_stamp() << " " << name() << " out of reset"  <<endl;
    while(true) {
      wait(requestReceived);
      cout << sc_time_stamp() << " " << name() << " preparing first response on length " << tr->get_data_length() << endl;
      tr->set_response_status(tlm::TLM_OK_RESPONSE); 
      if (tr->get_command()==tlm::TLM_WRITE_COMMAND) {
	cout << sc_time_stamp() << " " << name() << " sending write response"  <<endl;
	phase = tlm::BEGIN_RESP;   
	tsocket->nb_transport_bw(*tr, phase, delay_time);
	wait();
      }
      else {
	for (auto i=1; i<tr->get_data_length()/wData; ++i) {
	  cout << sc_time_stamp() << " " << name() << " sending read response " << i << " of " << tr->get_data_length()/wData << endl;
	  phase = axi::BEGIN_PARTIAL_RESP;   
	  tsocket->nb_transport_bw(*tr, phase, delay_time);
	  cout << sc_time_stamp() << " " << name() << " waiting for response end"  <<endl;
	  wait(responseEndReceived);
	  cout << sc_time_stamp() << " " << name() << " got response end"  <<endl;
	  wait();
	}
	cout << sc_time_stamp() << " " << name() << " sending last read response " << endl;
	phase = tlm::BEGIN_RESP;   
	tsocket->nb_transport_bw(*tr, phase, delay_time);
	cout << sc_time_stamp() << " " << name() << " waiting for response end"  <<endl;
	wait(responseEndReceived);
	cout << sc_time_stamp() << " " << name() << " got response end"  <<endl;
	wait();
      }
      cout << sc_time_stamp() << " " << name() << " finished response set for transfer"  <<endl;
    }
  };
};

class testbench : public sc_core::sc_module {
public:
  SC_HAS_PROCESS(testbench);
  sc_core::sc_time         clk_period{ClockPeriodNS, sc_core::SC_NS};
  sc_core::sc_clock        clk{"clk", clk_period, 0.5, sc_core::SC_ZERO_TIME, true};
  sc_core::sc_signal<bool> rst{"rst"};
  MyMaster                 master1{"master1"};
  MySlave                  slave1 {"slave_1"};
  
  testbench(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    master1.Clk(clk);
    master1.Rst(rst);
    slave1.Clk(clk);
    slave1.Rst(rst);
    master1.isocket(slave1.tsocket);
  }

};

int sc_main(int argc, char* argv[]) {
    testbench tb("tb");
    tb.rst.write(true);
    sc_core::sc_start(int(10*ClockPeriodNS), SC_NS);
    tb.rst.write(false);
    sc_core::sc_start(10, SC_US);
    std::cout << "Finished by maximum simulation period";
    return 0;
};
