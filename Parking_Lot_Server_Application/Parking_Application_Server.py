#Preston Ott, 4/7/2023, OYSMS
#This will be update to the Parking_App_Server program so that it can read from a txt file updated by the receiver
#Importing the libraries needed for program
import threading
import time
import socket
import Server_Basics
import sqlite3
import math

#This is a update on Parking_App_Server_V2 that catches errors that possibly occur on transmission.


#Defining the path to our text file that will be written to by the receiver
sp_path = str(r"C:\Users\large\OneDrive\Documents\Desktop\Design Project\Server_Client_Programs\Server_Programs\Final_Server_Programs\spotarray.txt")
#Now we make a server class with a set of functions that will be utilized for the server program

class app_server():
    #The constructor simply defines the ip and port server will run on
    def __init__(self,host,port):
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
    #This will create a new socket ipv4 and listen for client request
    def create_socket(self):
         sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
         sock.setblocking(False)
         sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
         sock.bind((self.host, self.port))
         sock.listen()
         return sock

        
    #This will make a infinite loop that will connect clients and handle them in a new thread
    def listen(self):
        
         #Here we create a server socket and list address its listening on
         listen_sock = self.create_socket()
         addr = listen_sock.getsockname()
         print('Listening on {}'.format(addr))
         
         #Now we go into a infinite while loop and listen and handle multiple clients
         while(True):
             
             #Accept a new client connection
             try:
                 client_sock, addr = listen_sock.accept()
                 print("new client: ", client_sock, addr)
                 #Create a new thread for client
                 thread = threading.Thread(target = self.handle_client(client_sock,addr))
                 #Now we start executing thread NOT giving it a wait till done condition
                 thread.start()
             except(BlockingIOError):
                 pass
             
             #Now we check if database should be updated by transmitter
             if(check_text(sp_path) == 1):
                 #Getting the spotarray
                 sp = fetch_data(sp_path)
                 #Storing it in database
                 update_database(sp)
                 print(sp)
                 #Now we clear the contents of the text file
                 clear_text(sp_path)
                 
                
                 
             

    #Here we make a function that will handle the clients, accept request, parse request, fetch data from database, format response, send reponse
    def handle_client(self,client_sock,addr):
         #In handle client we make a new thread to handle the client
         try:
             #Receiving raw message from client
             msg = self.accept_request(client_sock)
             #Parsing the received message (gets address)
             addr = self.parse_request(msg)
             #Now using the database and address we get the spot array
             sp = self.fetch_data(addr)
             #Now we format the response
             resp = self.format_response(sp)
             #Now we send the response and close the client connection
             self.send_response(client_sock,resp)
             
        #If error occurrs we just print out a error occured
         except (ConnectionError, BrokenPipeError):
             print('Error occured')
             
             


    #Here we define a function that will accept the request
    def accept_request(self,client_sock):
        try:
            #Recieving request of 64 bits
            req = client_sock.recv(120)
            req = req.decode('utf-8')
            return req
        except(ConnectionError):
            print("connection error")
            return ""
        

    #Here we make a function to parse the request
    def parse_request(self,req):
        
        #Extracting the /spotarray/(addr) of request
        type = ""
        for i in range(11):
            type = type + req[i]
        
        #Checking if its the valid type
        if(type == "/spotarray/"):
            addr = ""
            for j in range(len(req) - 11):
                addr = addr+req[11+j]
            return addr
        else:
            return "Invalid Request"
        
        
    #Here we make a function that will fetch data from the database using the address we recieved from client
    def fetch_data(self,addr):
        
        #Here we connect to our database
        park_base1 = sqlite3.connect("park_base1.db")
        #Make a cursor for the database
        c = park_base1.cursor()
        
        #Now we get the table of all the parking lots
        c.execute("SELECT * FROM parkinglots")
        #Now we fetch all the entrys of the Table
        entrys = c.fetchall()
        
        #Now we fetch all the addresses
        addresses = []
        for i in range(len(entrys)):
            addresses.append(entrys[i][0])
            
        #Now we check if the address is in the list
        in_list = addresses.count(addr)
        if(in_list == 0):
            return "Invalid_Address"
        else:
            index = addresses.index(addr)
            return entrys[index][2]
        

    #Formatting a spot array response with the spot array integer
    def format_response(self,sp_int):
        resp = "/spotarray/"
        resp = resp + str(sp_int)
        #Encoding the msg, making 64 bits long
        resp = resp.encode('utf-8')
        resp = resp + b''*(120-len(resp))
        return resp


    #Now we make a function to send the spot-array response
    def send_response(self,client_sock,resp):
        
        #Sending the response to client
        client_sock.sendall(resp)
        #Ending connection
        client_sock.close()


#Making 4 functions that will be utilized to update the sqlite3 database
        
#Checks if text file is empty or populated
def check_text(path):
    f1 = open(path,"r")
    txt = f1.readline()
    if(txt == ""):
        return 0
    else:
        return 1

#Now we make function that will fetch the data and convert it into a integer
def fetch_data(path):
    #Putting this all in a try except error statement to catch Uni-Code Errors
    try:
        
        #Getting spot array
        f1 = open(path,"r")
        sp = f1.readline()
        size = len(sp)
        
        #Now converting the string into a decinal number
        spot = 0
        for i in range(size):
            spot = math.pow(2,i)*int(sp[i]) + spot
        
        #Now we return the spot array integer, later we will also return its size
        return int(spot)
    #Unicode error occurs, delete file to get rid of faulty data
    except(UnicodeError):
        clear_text(path)
        
        

#Now we make a function to update the spot array integer for the particular address, only 1 address currently is used
def update_database(sp):
    
    #Opening the park_base1 database
    base = sqlite3.connect('park_base1.db')
    c = base.cursor()
    
    #Now we write (sp) int into our database
    s1 = "UPDATE parkinglots SET spot_array ="
    s2  = s1 + str(sp) + " WHERE rowid = 1"
    c.execute(s2)
    base.commit()
    base.close()
    

#Here is a function for clearing the text in the text file
def clear_text(path):
    #Opening it in write mode clears it
    f1 = open(path,'w')
    f1.close()

        


    
    
    
        

#Here is our main function that will handle client connections, handling request,fetching data, generating responses, sending responses for all clients
if __name__ == "__main__":
    #Scheduling to call this function every 0.5s to update database with receiver info
    #Making a server object with our Host = 192.168.1.147 and Port = 9999
    server1 = app_server('172.24.124.251',9999)
    #Running the listening function
    server1.listen()
