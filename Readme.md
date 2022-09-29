
- Things we need:
sudo apt install g++ gcc cmake openssl git libaio1 libaio-dev

- Install libwebsockets:
git clone https://github.com/warmcat/libwebsockets.git

cd /path/to/src
mkdir build
cd build
cmake ..
make && sudo make install

- Download and run codebase:
git clone https://github.com/gregtriant/embedded2022.git
cd embedded2022
make
./client