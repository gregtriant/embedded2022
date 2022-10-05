
# Things we need:
sudo apt install g++ gcc cmake openssl git libaio1 libaio-dev

# Install boost cpp library
sudo apt-get install libboost-all-dev

# Install libwebsockets:
git clone https://github.com/warmcat/libwebsockets.git  
cd /path/to/src  
mkdir build  
cd build  
cmake ..  
make && sudo make install

# Download and run codebase:
git clone https://github.com/gregtriant/embedded2022.git  
cd embedded2022  
make  

# To run the process from ssh in the background type:
nohup ./client &  

# To check the log later
cat nohup.out  

