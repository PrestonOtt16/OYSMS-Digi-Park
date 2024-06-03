//The entirety of this code is taken from github
//Link https://github.com/hallard/RadioHead/tree/master/examples/raspi/rf95
//Later on we will set the reciever to get messages from certain nodes allowed by us

#include <bcm2835.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
using namespace std;

#include <RH_RF69.h>
#include <RH_RF95.h>

//Pin definitions
#define RF_CS_PIN  RPI_V2_GPIO_P1_24 //Defines the chip select for the transmitter to be on GPIO pin 24 on the raspberry pi
#define RF_IRQ_PIN RPI_V2_GPIO_P1_22 //Defines the interupt dependant pin for the transmitter to be on GPIO pin 24 on the raspberry pi
#define RF_RST_PIN RPI_V2_GPIO_P1_15 //Defines the reset pin for the transmitter to be on GPIO pin 24 on the raspberry pi

// Our RFM95 Configuration 
#define RF_FREQUENCY  910.00 //Defines transmission frequency to be defined as 868 (Mhz)
#define RF_NODE_ID    1      //Defines the node ID of this reciever that we are using

//Create an instance of a driver
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
  
  //Returns the pointer to sig_handler() when SIGINT is interrupted by the force_exit flag
  //This function is used to stop the program after Ctrl-C is used in the Linux terminal
  signal(SIGINT, sig_handler);
  printf( "%s\n", __BASEFILE__);

  ////checks to see if the bcm2835 library has been successfully initialized (so that the GPIO pins and SPi can be used on the Raspi)
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

  //Enables Rising edge detection for the specific pin
  bcm2835_gpio_ren(RF_IRQ_PIN);
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

    //This function sets the power output to 14dBm (Can range from 2dBm to 20dBm for our module)
    //The boolean is used to distinguish how the transmitter module sets the power output
    //Since this library was written for several modules
    rf95.setTxPower(14, false);

    //This function sets the frequency of the module (returns true if the frequency is within range)
    //Our module has the potential to set the frequency from 137Mhz to 1020Mhz
    //This module was manufactured to work best at either 868Mhz or 915Mhz
    rf95.setFrequency(RF_FREQUENCY);
    

    //This function sets the address of our node (transmitter)
    //The node ID is used to test the address in incoming messages on the reciever side
    //Allows for multi nodes for one reciever (all nodes have unique IDs)
    rf95.setThisAddress(RF_NODE_ID);
    //Set HeaderFrom as the same Node ID to prevent address spoofing
    rf95.setHeaderFrom(RF_NODE_ID);
    /*^^^^ The above two functions are only needed for transmission                           ^^^^*/
    /*^^^^ We may opt into making this both a reciever and transmitter later into the project ^^^^*/
    

    //This function tells the receiver to accept messages with any TO address 
    //not just messages addressed to thisAddress or the broadcast address (i referenced this function in the transmitter code)
    //Since we are testing functionality we set this as true to make sure the module works
    //In a more practical sense we would set this as false to recieve messages from our specific transmitters
    rf95.setPromiscuous(true);


    //This function changes the module to be in recieve mode (as opposed to transmit or idle)
    rf95.setModeRx();


    printf( " OK NodeID=%d @ %3.2fMHz\n", RF_NODE_ID, RF_FREQUENCY );
    printf( "Listening packet...\n" );

    //Begin the main body of code
    while (!force_exit) 
    {
      
#ifdef RF_IRQ_PIN
      
      //This is the Event Detect Status function which tests whether the specified pin has detected a level or edge
      //Returns true if the Event Detect Status is High
      if (bcm2835_gpio_eds(RF_IRQ_PIN)) 
      {
        //This function clears the Event Detect Status flag
        bcm2835_gpio_set_eds(RF_IRQ_PIN);

#endif

        //This function checks to see if a new message is available from the Driver 
        //Returns true if a new, complete, error-free uncollected message is available to be retreived by the recv() function
        if (rf95.available()) 
        { 

          /* Should be a message for us now */

          //RH_RF95_MAX_MESSAGE_LEN is the maximum message length that can be suported by this driver
          //Defined as 1 byte message length, 4 bytes headers, user data and 2 bytes of FCS in RH_RF95.h
          uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
          uint8_t len   = sizeof(buf);

          //Puts the FROM header of the last received message into from
          uint8_t from  = rf95.headerFrom();
          //Puts the TO header of the last received message
          uint8_t to    = rf95.headerTo();
          //Puts the ID header of the last received message into id
          uint8_t id    = rf95.headerId();
          //Puts the FLAGS header of the last received message into flags
          uint8_t flags = rf95.headerFlags();;
          /*^^^^ The above 4 parameters can be used to make sure we recieve information from nodes that we allow ^^^^*/
          /*^^^^ Note that every message sent from every RFM95W module has these 4 headers sent in every message ^^^^*/

          //Puts the signal strength of the last message recieved (in dBm) into rssi
          int8_t rssi   = rf95.lastRssi();
          
          
          if (rf95.recv(buf, &len)) 
          {
            
            //Prints the information of the node we recieved a message from
            printf("Packet[%02d] #%d => #%d %ddBm: ", len, from, to, rssi);
            
            //Prints a data buffer in HEX. For diagnostic use
            printbuffer(buf, len);
            
            //Uses the ofstream library to open and write to a textfile
            ofstream out;
            
            //Turns the data of buf into a char to write to the text file
            char* new_data = reinterpret_cast<char *>(buf); 
            
            //The two functions below perform the writing to the text file
            out.open("/home/zackspine/RadioHead/examples/raspi/rf95/spotarray");
            out.write(new_data,len);
            
            //Closes the text file after use
            out.close();
            
          } else 
          {
            Serial.print("receive failed");
          }
          printf("\n");
        }
        
#ifdef RF_IRQ_PIN
      }
#endif
      
      //Lets the OS do other tasks
      //Since we do nothing until every 5 sec
      //the actual funciton delays for the specified number of milliseconds using nanosleep()
      bcm2835_delay(5);
    }
  }

  printf( "\n%s Ending\n", __BASEFILE__ );

  //this function closes the library, deallocating any allocated memory
  bcm2835_close();

  return 0;
}
