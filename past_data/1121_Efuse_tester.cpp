#include "testmethod.hpp"
#include "mapi.hpp"
#include "rdi.hpp"
#include "Common_Utilities.hpp"
#include "RA_protocols.h"
#include "JTAG_Protocol.cpp"
using namespace std;
using namespace RA;


class Efuse_wr_rd: public testmethod::TestMethod {

protected:

virtual void initialize()
{

}

virtual void run()
{
	RDI_INIT();

	ON_FIRST_INVOCATION_BEGIN();

// need to uncomment for online tester
	dps_connect();
	rdi.hwRelay().pin("ALL_DIG_PINS").setOff("ALL").execute();
	rdi.hwRelay().pin("ALL_DIG_PINS").setOn("AC").execute();
	rdi.wait(2 mS);

	rdi.dc("vdd_core").pin("vdd_core").vMeas().vMeasRange(2.25).average(16).measWait(1 ms).execute();





		//efuse_addr		    =        0x0  				/*0x1*/				/*0x2*/           			/*0x63*/;		   // 6 bit
		//uint64_t efuse_data = 0x280000201234ABCD  /*0x6800002012340000*/  /*0x58000020123abcdf*/  /*0xabcd123498765432*/;  // 64 bit

		//efuse_addr		    =        /*0x10*/  				/*0x20*/				/*0x30*/           			0x40;		   // 6 bit
		//uint64_t efuse_data = /*0x100000201234AB10*/  /*0x2000002012340020*/  /*0x30000020123abc30*/ 0x40cd123498765440;  // 64 bit

		int error = 0,efuse_addr=0;uint64_t efuse_data =0;


//		efuse_addr = 0x0 ;efuse_data = 0x280000201234ABCD ;
//		write_efuse(efuse_addr,efuse_data,false,1);
//		read_efuse(efuse_addr,efuse_data,error,2);

//		efuse_addr = 0x1 ;efuse_data = 0x6800002012340000 ;
//		write_efuse(efuse_addr,efuse_data,false,3);
//		read_efuse(efuse_addr,efuse_data,error,4);

//		efuse_addr = 0x2 ;efuse_data = 0x58000020123abcdf ;
//		write_efuse(efuse_addr,efuse_data,false,5);
//		read_efuse(efuse_addr,efuse_data,error,6);

//		efuse_addr = 0x63 ;efuse_data = 0xabcd123498765432 ;
//		write_efuse(efuse_addr,efuse_data,false,7);
//		read_efuse(efuse_addr,efuse_data,error,8);*/


/*		efuse_addr = 0x10 ;efuse_data = 0x100000201234AB10 ;
//		write_efuse(efuse_addr,efuse_data,false,1);
		read_efuse(efuse_addr,efuse_data,error,2);

		efuse_addr = 0x20 ;efuse_data = 0x2000002012340020 ;
//		write_efuse(efuse_addr,efuse_data,false,3);
		read_efuse(efuse_addr,efuse_data,error,4);

		efuse_addr = 0x30 ;efuse_data = 0x30000020123abc30 ;
//		write_efuse(efuse_addr,efuse_data,false,5);
		read_efuse(efuse_addr,efuse_data,error,6);*/

/*		efuse_addr = 0x51 ;efuse_data = 0x40cd123498765440 ;
//		write_efuse(efuse_addr,efuse_data,false,7);
		read_efuse(efuse_addr,efuse_data,error,8);


		efuse_addr = 0x50 ;efuse_data = 0x1044611ABCD43219 ;
//		write_efuse(efuse_addr,efuse_data,false,1);
		read_efuse(efuse_addr,efuse_data,error,2);

		efuse_addr = 0x40 ;efuse_data = 0x1144612ABCD43228 ;
//		write_efuse(efuse_addr,efuse_data,false,3);
		read_efuse(efuse_addr,efuse_data,error,4);

		efuse_addr = 0x52 ;efuse_data = 0x1244613ABCD43237 ;
//		write_efuse(efuse_addr,efuse_data,false,5);
		read_efuse(efuse_addr,efuse_data,error,6);*/

	 for (int i = 0; i <= 0x63; i++) {
			// Convert the value to a string in hexadecimal format
			std::ostringstream oss;
			oss << std::hex << i;
			std::string hexStr = oss.str();

			// Check if the hexadecimal representation contains 'a', 'b', 'c', 'd', 'e', or 'f'
			if (hexStr.find('a') != std::string::npos ||
				hexStr.find('b') != std::string::npos ||
				hexStr.find('c') != std::string::npos ||
				hexStr.find('d') != std::string::npos ||
				hexStr.find('e') != std::string::npos ||
				hexStr.find('f') != std::string::npos) {
				continue; // Skip this value
			}

			// Print the allowed hexadecimal value
			std::cout << "efuse_addr (hex): 0x" << std::hex << i << std::endl;
			efuse_addr = i ;efuse_data = 0 ;

			//write_efuse(i,efuse_data,false,5);
			read_efuse(i,efuse_data,error,6);
		}


	ON_FIRST_INVOCATION_END();


  return;
}

void read_efuse(int efuse_addr, uint64_t efuse_data, int& error,int id)
{

	cout <<endl<< "*********************read_efuse*********************" << endl;
	cout << "Efuse Address : " << hex << efuse_addr << endl;
	cout << "Efuse Data    : " << hex << efuse_data << endl;
	cout << 	  "****************************************************" << endl<<endl;
	// read_efuse
	int chk, CHIP_EFUSE_REGION = 0x2c100000;
	chk =0;
	error = 0;
	cout << "\n>>--> Inital error " << error << endl;
	cout << "\n >>  SB to Efuse Read" << endl << endl;

	// Write efuse ctrl reg

	jtag_wr(0b00,CHIP_EFUSE_REGION,0x01f40002); //added 01f4 9/18
	cout << ">>--> Write efuse ctrl reg" << endl << endl;

  //Write efuse status reg (Clear status register)

	jtag_wr(0b00,CHIP_EFUSE_REGION+0x08,0x0000FFFF);
	cout << ">>--> Clear status register" << endl << endl;

	//Write efuse addr reg (set address to read data)

	jtag_wr(0b00,CHIP_EFUSE_REGION+0x0C, efuse_addr & 0x3F); //9/18
	//cout << "CHIP_EFUSE_REGION+0x0C : " <<hex<< CHIP_EFUSE_REGION+0x0C << endl;
	cout << ">>--> set address to read data" << endl << endl;

	//Write efuse start reg with read efuse

	jtag_wr(0b00,CHIP_EFUSE_REGION+0x04,0x00000002);
	cout << ">>--> Write efuse start reg with read efuse"  << endl << endl;

	repeat(10) ;

	//check status reg with 02: 9/18
  // Read efuse status reg (check read completion bit)
	jtag_rd(0b00, CHIP_EFUSE_REGION + 0x08, 0x00000002,chk,200+efuse_addr);
	cout << ">>--> check read completion bit" << endl;
  if (chk) error = 1;
  cout << "\n>>--> check read completion bit error " << error << endl<< endl;

	//Read efuse data low reg

	jtag_rd(0b00, CHIP_EFUSE_REGION+0x18, efuse_data ,chk, 300+efuse_addr);
	cout << ">>--> Read efuse data low reg" << endl;
	if (chk) error = 1;
	 cout << ">>-->  Read efuse data low reg error " << error << endl<< endl;

	//Read efuse data high reg

	jtag_rd(0b00,CHIP_EFUSE_REGION+0x1C, efuse_data >> 32, chk, 400+efuse_addr);
	cout << ">>--> Read efuse data high reg" << endl;
	if (chk) error = 1;
	 cout << ">>--> Read efuse data high reg error " << error << endl<< endl;

	 cout << ">>-->  Error : "<< error << endl <<endl;

}
void write_efuse(int efuse_addr,uint64_t efuse_data,Boolean burn,int id)
{

	cout <<endl<< "**********************write_efuse********************" << endl;
	cout << "Efuse Address : " << hex << efuse_addr << endl;
	cout << "Efuse Data    : " << hex << efuse_data << endl;
	cout <<       "*****************************************************" << endl<<endl;
	//write_efuse

	double value =0;

	value = Primary.getLevelSpec().getSpecValue("vdd_ef_1");
	cout << "Initial vdd_ef_1 value : " << value << endl <<endl;

	if(burn){ Primary.getLevelSpec().change("vdd_ef_1",1.8 V); FLUSH(TM::APRM);}
	value = Primary.getLevelSpec().getSpecValue("vdd_ef_1");
	cout << "Before writing vdd_ef_1 value : " << value << endl<<endl;

	int CHIP_EFUSE_REGION = 0x2c100000,
	error = 0,
	sb_data,chk =0 ;

	//Primary.getLevelSpec().change("vdd_ef_1",1.8);   	FLUSH(TM::APRM);

	//Select Muxes: Efuse + enable writing  + count = 500  (Write efuse ctrl reg)

	jtag_wr(0b00, CHIP_EFUSE_REGION, 0x01f40003);
	cout << ">>--> Write efuse ctrl reg" << endl << endl;

	//read efuse ctrl reg

	jtag_rd(0b00, CHIP_EFUSE_REGION, 0x01f40003,chk,100+efuse_addr);
	cout << ">>--> Read efuse ctrl reg" << endl << endl;
	if (chk) error = 1;

	//Write efuse status reg (Clear status register)

	jtag_wr(0b00, CHIP_EFUSE_REGION+0x08, 0x0000FFFF);
	cout << ">>--> Clear status register" << endl << endl;

  //Write efuse addr reg (set address to write data)

	jtag_wr(0b00, CHIP_EFUSE_REGION+0x0C, efuse_addr & 0x3F);
	cout << ">>--> Write efuse addr reg" << endl << endl;

  // Write efuse data high reg (clear data high)

	jtag_wr(0b00, CHIP_EFUSE_REGION+0x14, 0);
	cout << ">>--> clear data high" << endl << endl;

	for (int j = 0; j < 32; j++)
	{
	   sb_data = efuse_data & (1 << j);  // Shift and mask to get the j-th bit
	   cout << "write_efuse I : " << dec << j << " | sb_data: " << toBinaryString(sb_data, 32) << endl;
	   cout << endl;
		if(sb_data != 0)
		{
			//Write one bit at a time (only one fuse can be burned within a burn cycle)
			//Write efuse data low reg
			jtag_wr(0b00, CHIP_EFUSE_REGION+0x10, sb_data);
			cout << ">>--> Write efuse data low reg" << endl;

			//Write efuse start reg with write efuse

			jtag_wr(0b00, CHIP_EFUSE_REGION + 0x04, 0x00000001);
			cout << ">>--> Write efuse start reg with write efuse"  << endl;

			//9/18:add wait of >10us and check status reg and clr
			rdi.port("pCOLD_BOOT_Port").wait(15 us);

		    // Read efuse status reg (check write completion bit)
			jtag_rd(0b00, CHIP_EFUSE_REGION + 0x08, 0x00000001,chk,(efuse_addr*1000)+j);
			cout << ">>--> check write completion bit" << endl;
		    if (chk) error = 1;

			//Write efuse status reg (Clear status register)
			jtag_wr(0b00, CHIP_EFUSE_REGION+0x08, 0x0000FFFF);
			cout << ">>--> Clear status register" << endl << endl;

		}
	 }
	cout << endl;

	//Write efuse data low reg (clear data low)

	jtag_wr(0b00, CHIP_EFUSE_REGION+0x10, 0);
	cout << ">>--> Write efuse data low reg" << endl << endl;

	for (int j = 0; j < 32; j++)
	{
	   sb_data = (efuse_data >> 32 ) & (1 << j);  // Shift and mask to get the j-th bit
	   cout << "write_efuse I : " << dec << j << " | sb_data: " << toBinaryString(sb_data, 32) << endl;
	   cout << endl;
		if(sb_data != 0)
		{
			//Write one bit at a time (only one fuse can be burned within a burn cycle)
			//Write efuse data high reg
			jtag_wr(0b00, CHIP_EFUSE_REGION+0x14, sb_data);
			cout << ">>--> Write efuse data high reg" << endl;

			//Write efuse start reg with write efuse

			jtag_wr(0b00, CHIP_EFUSE_REGION + 0x04, 0x00000001);
			cout << ">>--> Write efuse start reg"  << endl;

			//9/18:add wait of >10us and check status reg and clr
			rdi.port("pCOLD_BOOT_Port").wait(15 us);

		    // Read efuse status reg (check write completion bit)
			jtag_rd(0b00, CHIP_EFUSE_REGION + 0x08, 0x00000001,chk,(efuse_addr*2000)+j);
			cout << ">>--> Read efuse status reg" << endl;
		    if (chk) error = 1;

			//Write efuse status reg (Clear status register)
			jtag_wr(0b00, CHIP_EFUSE_REGION+0x08, 0x0000FFFF);
			cout << ">>--> Clear status register" << endl << endl;

		}
	 }
	//clear ctrl reg with 00
  // Write efuse data high reg (clear data high)
	jtag_wr(0b00, CHIP_EFUSE_REGION+0x14, 0);
	cout << ">>--> clear data high" << endl << endl;

	//Select Muxes: Efuse + enable writing  + count = 500  (Write efuse ctrl reg)
	jtag_wr(0b00, CHIP_EFUSE_REGION, 0x01f40000);
	cout << ">>--> Write efuse ctrl reg with 0x****0000" << endl << endl;

   repeat(400);

	if (error) cout << "Write error" << endl;

	if(burn) {Primary.getLevelSpec().change("vdd_ef_1",0 V); FLUSH(TM::APRM);}
	value = Primary.getLevelSpec().getSpecValue("vdd_ef_1");
	cout << "After writing vdd_ef_1 value : " << value << endl;

	}


void jtag_wr(int jtag_size,int jtag_addr,int jtag_data)
{
	// jtag_write
	 RDI_BEGIN();
	 {
		bitset<36> jtag_req; int combined_value;
		rdi.port("pCOLD_BOOT_Port").smartVec().burstLabel("jtag_write").begin();

		IR_func(0b000001,6,"LSB");  // 0b000001


		jtag_req[35] = 0; // err
		jtag_req[34] = 1; // req
		jtag_req[33] = 0; // rw_n
		jtag_req[32] = 1; // jtag_req[32] = ~beat[0]; // ad_n

		combined_value = (jtag_addr << 2) | (jtag_size & 0x3);
	    setBitRange(jtag_req, 0, 31, combined_value);

		DR_func(jtag_req.to_ulong(),36,"LSB");

		jtag_req[32] = 0; // jtag_req[32] = ~beat[0]; // ad_n setting req to data
		setBitRange(jtag_req, 0, 31, jtag_data);

		DR_func(jtag_req.to_ulong(),36,"LSB");

		rdi.port("pCOLD_BOOT_Port").smartVec().burstLabel().end();
	 }
	 RDI_END();
	 //cout << "*********jtag wr is completed**********" << endl;
}

void jtag_rd(int jtag_size,int jtag_addr,int jtag_data,int& err,int id)
	{
	RDI_BEGIN();
	{
		bitset<36> jtag_req; int combined_value;
		rdi.port("pCOLD_BOOT_Port").smartVec().burstLabel("jtag_read_addr_data").begin();

		IR_func(0b000001,6,"LSB");  // 0b000001

		jtag_req[35] = 0; // err
		jtag_req[34] = 1; // req
		jtag_req[33] = 1; // rw_n
		jtag_req[32] = 1; // ad_n

		combined_value = (jtag_addr << 2) | (jtag_size & 0x3);
		setBitRange(jtag_req, 0, 31, combined_value);
		DR_func(jtag_req.to_ulong(),36,"LSB");

		IR_func(0b000010,6,"LSB");

		read_func(0x0,1,"LSB",0,36,"addr"+rdi.itos(id)); //check err bit[35] is 0 after this and return addr is matching

		//confirm the set addr is read back
		IR_func(0b000010,6,"LSB");

		read_func(0x0,1,"LSB",0,36,"data"+rdi.itos(id));//check err bit[35] is 0 after this and return data is matching

		rdi.port("pCOLD_BOOT_Port").smartVec().burstLabel().end();
	}
	RDI_END();
	// cout << "*********jtag rd is completed**********" << endl;
	// cout << "************Result_Retrieval**********" << endl;

	ARRAY_I cap_addr,cap_data;
	bitset<36> jtag_Addr, jtag_Data;

	cap_addr = rdi.id("addr"+rdi.itos(id)).getReadBit("tap_tdo_pad_o");
	cap_data = rdi.id("data"+rdi.itos(id)).getReadBit("tap_tdo_pad_o");

	cout <<"Size : " << dec << cap_addr.size() << " | cap_addr : " << cap_addr << endl;
	cout <<"Size : " << dec << cap_data.size() << " | cap_data : " << cap_data << endl;

	 for(int j=0;j<36;j++)
	 {
		 jtag_Addr[j] = cap_addr[j];         //storing jtag addr
		 jtag_Data[j] = cap_data[j];      	 //storing jtag data
	 }

	 result_retrieval(jtag_Addr,jtag_Data,err,jtag_addr,jtag_data);

	}

void Reverse_bit(int bitSize,uint64_t data,uint64_t& reversedDecimal)
	{
	 if (bitSize == 0 || bitSize > 64)
	  {
	        cout << "Invalid bitSize. Must be between 1 and 64." << endl;
	  }
	    // Use a bitset with a maximum of 64 bits
	    bitset<64> binaryOriginal(data);  // Convert to 64-bit binary representation

	    // Extract only the relevant bits
	    string binaryString = binaryOriginal.to_string().substr(64 - bitSize, bitSize);

	    // Reverse the binary string
	    reverse(binaryString.begin(), binaryString.end());

	    // Convert the reversed binary string back to a decimal integer
	    //unsigned long long int reversedDecimal = bitset<64>(binaryString).to_ullong();
	    reversedDecimal = bitset<64>(binaryString).to_ulong();

	}


 void setBitRange(bitset<36>& bitset, int start, int end, unsigned int value)
 {
     for (int i = start; i <= end; ++i){
         bitset[i] = (value >> (i - start)) & 1;}
 }

 void result_retrieval(bitset<36>jtag_Addr,bitset<36>jtag_Data,int& err,int jtag_addr,int jtag_data)
 {
 	 bitset<30> Addr;  bitset<32> Data;
 	 cout << "cap_addr : " << jtag_Addr << endl;
 	 cout << "cap_data : " << jtag_Data << endl;
 	 int dec_addr=0,dec_data=0;

 	 //////////////////////////////////////////checking jtag address

 	 if (jtag_Addr[35] != 0){
 	  cout << "ERROR>>> bad JTAG Response - Error returned by SB" << endl;
 	  cout << "Expected = 0 ||" << "Actual = "<< jtag_Addr[35] << endl;
 	  err =1;}

 	 if (jtag_Addr[34] != 0){
 	  cout << "ERROR>>> bad JTAG Response - Req/Rsp miscompare" << endl;
 	  cout << "Expected = 0 ||" << "Actual = "<< jtag_Addr[34] << endl;
 	  err =1;}

 	 if (jtag_Addr[33] != 0){   //1810
 	  cout << "ERROR>>> bad JTAG Response - R/W miscompare" << endl;
 	  cout << "Expected = 1 ||" << "Actual = "<< jtag_Addr[33] << endl;
 	  err =1;}

 	 if (jtag_Addr[32] != 1){
 	  cout << "ERROR>>> bad JTAG Response - A/D miscompare" << endl;
 	  cout << "Expected = 1 ||" << "Actual = "<< jtag_Addr[32] << endl;
 	  err =1;}

 	 Addr = (jtag_Addr >> 2).to_ulong();  // left by two and store in biset<30>
 	 dec_addr = Addr.to_ulong();          // converting the decimal
 	 //cout << "dec : Expected addr : " << jtag_addr << " || Actual : " << dec_addr << endl;
 	 cout << "hex : Expected addr : " << hex << jtag_addr << " || Actual : " << hex << dec_addr << endl;

 	 if(dec_addr != jtag_addr){
      cout << "ERROR>>> bad JTAG Response - Bus miscompare" << endl;
 	 cout << "Expected = "<< hex << jtag_addr <<" || " << "Actual = "<< hex << dec_addr << endl;
 	 err =1;}


 	 //////////////////////////////////////////checking jtag data

 	 if (jtag_Data[35] != 0){
 	  cout << "ERROR>>> bad JTAG Response - Error returned by SB" << endl;
 	  cout << "Expected = 0 ||" << "Actual = "<< jtag_Data[35] << endl;
 	  err =1;}

 	 if (jtag_Data[34] != 0){
 	  cout << "ERROR>>> bad JTAG Response - Req/Rsp miscompare" << endl;
 	  cout << "Expected = 0 ||" << "Actual = "<< jtag_Data[34] << endl;
 	  err =1;}

 	 if (jtag_Data[33] != 0){   //1810
 	  cout << "ERROR>>> bad JTAG Response - R/W miscompare" << endl;
 	  cout << "Expected = 1 ||" << "Actual = "<< jtag_Data[33] << endl;
 	  err =1;}

 	 if (jtag_Data[32] != 0){
 	  cout << "ERROR>>> bad JTAG Response - A/D miscompare" << endl;
 	  cout << "Expected = 0 ||" << "Actual = "<< jtag_Data[32] << endl;
 	  err =1;}

 	 Data = jtag_Data.to_ulong();         // store in biset<32>
 	 dec_data = Data.to_ulong();          // converting the decimal
 	// cout << "dec : Expected data : " << jtag_data << " || Actual : " << dec_data << endl;
 	 cout << "hex : Expected data : " << hex << jtag_data << " || Actual : " << hex << dec_data << endl;

 	 if(dec_data != jtag_data){
      cout << "ERROR>>> bad JTAG Response - Bus miscompare" << endl;
 	 cout << "Expected = "<< hex << jtag_data <<" || " << "Actual = "<< hex << dec_data << endl;
 	 err =1;}

 }


void IR_func(long IR_data,int IR_data_size,string order)
{
	  int exit_size = 3, shift_size =4;
	  uint64_t value;
	 // cout << "IR data size : " << dec <<IR_data_size  << " | Order :" << order<< endl;
	 // cout << "IR_data Actual  : " << toBinaryString(IR_data, IR_data_size) << endl;
	  if(order == "LSB")
	  {
		  Reverse_bit(IR_data_size,IR_data,value);
		  IR_data = value;
	 // cout << "IR_data Sending : " << toBinaryString(IR_data, IR_data_size)<< endl;
	  }

	  rdi.port("pCOLD_BOOT_Port").smartVec().pin("tap_tck_pad_i").defaultBit("P")
		//					        							shift IR             					exit IR
		  .pin("tap_tms_pad_i").defaultBit("0").writeData(0xc, shift_size, 0).writeData(0x6, exit_size, IR_data_size+exit_size)
		//										shift IR data
		  .pin("tap_tdi_pad_i").writeData(IR_data, IR_data_size, shift_size).defaultBit("0").execute();
}

void DR_func(long DR_data,int DR_data_size,string order)
{
	  int size = 3;
	  uint64_t value;
	  cout << "DR data size : " << dec << DR_data_size  << " | Order :" << order<< endl;
	  cout << "DR_data Actual  : " << toBinaryString(DR_data, DR_data_size) << endl;
	  if(order == "LSB")
	  {
		  Reverse_bit(DR_data_size,DR_data,value);
		  DR_data = value;
		  cout << "DR_data Sending : " << toBinaryString(DR_data, DR_data_size) << endl;
	  }

	  rdi.port("pCOLD_BOOT_Port").smartVec().pin("tap_tck_pad_i").defaultBit("P")
		//	 					                           shift DR			          	exit DR
	   .pin("tap_tms_pad_i").defaultBit("0").writeData(0x4,size,0).writeData(0x6,size,size+DR_data_size-1)
		//													shift DR data
	   .pin("tap_tdi_pad_i").defaultBit("0").writeData(DR_data, DR_data_size, size).execute();
}

void read_func(long DR_data,int DR_data_size,string order,int cap_delay,int cap_bit_len,string id)
{
	  int size = 3,exit;
	  if(cap_bit_len >= DR_data_size) exit = cap_bit_len; else exit = DR_data_size;
	  //cout << "DR data size : "<< dec << DR_data_size  << " | Order :" << order<< endl;
	  //cout << "DR_data Actual in read  : " << toBinaryString(DR_data, DR_data_size) << endl;
	  uint64_t value;
	  if(order == "LSB")
	  {
		  Reverse_bit(DR_data_size,DR_data,value);
		  DR_data = value;
		  //cout << "DR_data Sending in read : " << toBinaryString(DR_data, DR_data_size) << endl;
	  }

	  rdi.port("pCOLD_BOOT_Port").smartVec(id).pin("tap_tck_pad_i").defaultBit("P")
		//	 					                           shift DR			          	exit DR
	   .pin("tap_tms_pad_i").defaultBit("0").writeData(0x4,size,0).writeData(0x6,size,size+exit-1)
		//													shift DR data
	   .pin("tap_tdi_pad_i").defaultBit("0").writeData(DR_data, DR_data_size, size)
	   //                                 capturing the data
	   .pin("tap_tdo_pad_o").readBit(cap_bit_len,size+cap_delay).execute();
}

string toBinaryString(uint64_t num, int bits)
{
   ostringstream oss;
   for (int i = bits - 1; i >= 0; --i) {
       oss << ((num >> i) & 1);
}
   return oss.str();
}

void repeat(int count)
{
	 RDI_BEGIN();
	 {
		 rdi.port("pCOLD_BOOT_Port").smartVec().burstLabel("repeat").begin();

			 rdi.port("pCOLD_BOOT_Port").smartVec().pin("tap_tck_pad_i").defaultBit("P")
				 .pin("tap_tms_pad_i").defaultBit("0").writeData(0,count,0)
				 .pin("tap_tdi_pad_i").defaultBit("0").execute();

		 rdi.port("pCOLD_BOOT_Port").smartVec().burstLabel().end();
   }
	RDI_END();
}

};

REGISTER_TESTMETHOD("Efuse_wr_rd", Efuse_wr_rd);
