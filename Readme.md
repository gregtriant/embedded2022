sudo apt install g++
sudo apt install cmake
sudo apt intsall openssl
sudo apt install git

git clone https://github.com/warmcat/libwebsockets.git

cd /path/to/src
mkdir build
cd build
cmake ..
make && sudo make install

