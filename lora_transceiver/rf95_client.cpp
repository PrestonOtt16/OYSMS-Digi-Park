//The majority (all but one portion) of this code is taken from github
//Link https://github.com/hallard/RadioHead/tree/master/examples/raspi/rf95

#include <bcm2835.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

#include <RH_RF69.h>
#include <RH_RF95.h>

//Pin definitions
#define RF_CS_PIN  RPI_V2_GPIO_P1_24 //Defines the chip select for the transmitter to be on GPIO pin 24 on the raspberry pi
#define RF_IRQ_PIN RPI_V2_GPIO_P1_22 //Defines the interupt dependant pin for the transmitter to be on GPIO pin 24 on the raspberry pi
#define RF_RST_PIN RPI_V2_GPIO_P1_15 //Defines the reset pin for the transmitter to be on GPIO pin 24 on the raspberry pi

// Our RFM95 Configuration 
#define RF_FREQUENCY  915.00 //Defines transmission frequency to be defined as 915 (Mhz)
#define RF_GATEWAY_ID 1      //Defines the reciever ID that we are sending data too
#define RF_NODE_ID    10     //Defines the node ID of the transmitter that we are using

//Create an instance of a driver
//This can be used to operate several transmitter at a time on one device (raspi)
//RH_RF95 is a class defined in the RH_RF95.h header, we are defining rf95 as an object to be used for the functions
RH_RF95 rf95(RF_CS_PIN, RF_IRQ_PIN);

//Flag for Ctrl-C from the Linux terminal
volatile sig_atomic_t force_exit = false;

//Creates a function to be used in signal() that sets force_exit to be true
void sig_handler(int sig)
{
  printf("\n%s Break received, exiting!\n", __BASEFILE__);
  force_exit=true;
}

//Main Function
int main (int argc, const char* argv[] )
{

  //Creates long to be used for delay between messages
  static unsigned long last_millis;
  
  //Returns the pointer to sig_handler() when SIGINT is interrupted by the force_exit flag
  //This function is used to stop the program after Ctrl-C is used in the Linux terminal
  signal(SIGINT, sig_handler);

  //__BASE_FILE__ is a macro that expands to the name of the main input file, in the form of a C string constant.
  //This is the source file that was specified on the command line of the preprocessor or C compiler.
  printf( "%s\n", __BASEFILE__);

  //checks to see if the bcm2835 library has been successfully initialized (so that the GPIO pins and SPi can be used on the Raspi)
  //If not then stop the program and print that bcm2835 has failed (useful for debugging)
  if (!bcm2835_init()) 
  {
    fprintf( stderr, "%s bcm2835_init() Failed\n\n", __BASEFILE__ );
    return 1;
  }
  
  printf( "RF95 CS=GPIO%d", RF_CS_PIN);

#ifdef RF_IRQ_PIN
  printf( ", IRQ=GPIO%d", RF_IRQ_PIN );
  //Sets the IRQ pin as input and pull down
  pinMode(RF_IRQ_PIN, INPUT);
  bcm2835_gpio_set_pud(RF_IRQ_PIN, BCM2835_GPIO_PUD_DOWN);
#endif
  
#ifdef RF_RST_PIN
  printf( ", RST=GPIO%d", RF_RST_PIN );
  //Pulse a reset on module (writing low, delay, then high to the pin)
  pinMode(RF_RST_PIN, OUTPUT);
  digitalWrite(RF_RST_PIN, LOW );
  bcm2835_delay(150);
  digitalWrite(RF_RST_PIN, HIGH );
  bcm2835_delay(100);
#endif

  //Checks to see if the hardware successfully initialized
  //If not then stop the program and print that the RF95w module has failed (useful for debugging)
  if (!rf95.init()) 
  {
    fprintf( stderr, "\nRF95 module init failed, Please verify wiring/module\n" );
  } else 
  {
    printf( "\nRF95 module seen OK!\r\n");


#ifdef RF_IRQ_PIN
    // Since we may check IRQ line with bcm_2835 Rising edge detection
    // In case the radio already has a packet, the IRQ pin will stay high and will never go low (to never fire again) 
    // Except if we clear IRQ flags and discard one if any by checking
    rf95.available();
    /*  
    The actual use of the function checks to see if a new message is available from the Driver 
    Returns true if a new, complete, error-free uncollected message is available to be retreived by the recv() function
    */

    //Enables Rising edge detection for the specific pin
    bcm2835_gpio_ren(RF_IRQ_PIN);
#endif

    //This function sets the power output to 14dBm (Can range from 2dBm to 20dBm for our module)
    //The boolean is used to distinguish how the transmitter module sets the power output
    //Since this library was written for several modules
    rf95.setTxPower(14, false); 

    //This function sets the frequency of the module (returns true if the frequency is within range)
    //Our module has the potential to set the frequency from 137Mhz to 1020Mhz
    //This module was manufactured to work best at either 868Mhz or 915Mhz
    //Ours will be set at 868
    rf95.setFrequency( RF_FREQUENCY );

    
    //This function sets the address of our node (transmitter)
    //The node ID is used to test the address in incoming messages on the reciever side
    //Allows for multi nodes for one reciever (all nodes have unique IDs)
    rf95.setThisAddress(RF_NODE_ID);
    //Set HeaderFrom as the same Node ID to prevent address spoofing
    rf95.setHeaderFrom(RF_NODE_ID);
    //Sets HeaderTo as the ID of the reciever we want to send data too
    rf95.setHeaderTo(RF_GATEWAY_ID);
    /*^^^^ The above three functions will be used in conjunction with the reciever to recieve messages from specific nodes                     ^^^^*/  
    /*^^^^ There is a function (setPromiscuous();) used by the reciever to set the module to recieve messages from any node or specific nodes  ^^^^*/

    printf("RF95 node #%d init OK @ %3.2fMHz\n", RF_NODE_ID, RF_FREQUENCY );

    //Sets last_millis to millis()
    //millis() returns the # of milliseconds since this program used one of the bcm2835 functions
    //This will be used to add delay between messages
    last_millis = millis();

    //Begin the main body of transmission code
    while (!force_exit) 
    {

      //Send data every 5 seconds
      if ( millis() - last_millis > 5000 ) 
      {
        //Resets the timer
        last_millis = millis();
        
        //Creates a string to place the contents of the text file into
        string Read_Arr;

        //Buffer will be used to add characters to the string Read_Arr
        string buffer;
        uint8_t len = 0;
        
        //Uses the ifstream library to open the text file with the spot array
        ifstream in;
        in.open("/home/user1/Desktop/Testcode/test");
        
        //Looks through the whole text file until there are no more characters
        while(!in.eof())
        {
          getline(in, buffer);
          Read_Arr += buffer + "\n";
        }
        
        //Puts the contents of Read_Arr into a vector variable
        vector<uint8_t> copydata(Read_Arr.begin(), Read_Arr.end());
        for(auto it = copydata.begin(); it != copydata.end(); it++)
          len++;
        
        //Formats the vector into a uint8_t data type, which is used by the transmitter
        uint8_t * data = &copydata[0];
        len = len-2;
        
        //Closes the text file
        in.close();
        
        //Sets the data that we will be sending to the reciever
        //In our case we want to send a bunch of 1's and 0's as determined by the ML model for parking spot detection
        //uint8_t data[] = "Hi Raspi!";
        //Sets the length of the data we want to send
        //uint8_t len = sizeof(data);
        
        //Confirms the data we want to send by printing on the linux terminal
        printf("Sending %02d bytes to node #%d => ", len, RF_GATEWAY_ID );
        
        
        rf95.send(data, len);
        //Prints a data buffer in HEX. For diagnostic use
        printbuffer(data, len);
        printf("\n" );

        //This function waits until previous transmit packet is finished being transmitted with waitPacketSent()
        //Then loads a message into the transmitter and starts the transmitter
        //rf95.send(data, len);
        //Blocks until the transmitter is no longer transmitting
        rf95.waitPacketSent();
        
      }
      
      //Lets the OS do other tasks
      //Since we do nothing until each 5 sec
      //the actual funciton delays for the specified number of milliseconds using nanosleep()
      bcm2835_delay(100);
    }
  }

  printf( "\n%s Ending\n", __BASEFILE__ );

  //this function closes the library, deallocating any allocated memory
  bcm2835_close();

  return 0;
}
