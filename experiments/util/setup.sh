cd ~

echo "[+] install tmux"
sudo apt install tmux -y

echo "[+] install deps"
sudo apt update && sudo apt-get install libboost-all-dev libgoogle-glog-dev software-properties-common -y
sudo add-apt-repository 'ppa:ubuntu-toolchain-r/test' && sudo apt update && sudo apt-get install gcc-13 g++-13 -y
sudo apt-get install -y doxygen pkg-config cmake ninja-build

echo "[+] build wasm-toolchain"
git clone https://github.com/fix-project/wasm-toolchain.git
cd wasm-toolchain
git submodule update --init --recursive
./build.sh

echo "[+] setup bootstrap"
cd ~
wget https://github.com/fix-project/bootstrap/releases/download/v0.93/bootstrap.tar.gz
tar -xzvf bootstrap.tar.gz

echo "[+] build fix"
cd ~
git clone https://github.com/fix-project/fix.git
cd fix
git checkout -b benchmark
git submodule update --init --recursive
cp -r ~/.fix ./
cmake -S . -B build
cmake --build build/ --parallel 256
