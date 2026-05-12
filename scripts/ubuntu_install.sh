#!/bin/bash

set -e  # Прерывать выполнение при ошибке

echo "========================================="
echo "Установка system dependencies (Ubuntu)"
echo "========================================="

# Обновление списка пакетов
sudo apt update

# Установка основных зависимостей
sudo apt install --yes --no-install-recommends \
    curl \
    ccache \
    python3 \
    python3-pip \
    python3-virtualenv \
    build-essential \
    ninja-build \
    cmake \
    git \
    clang \
    nodejs \
    npm

if ! command -v node &> /dev/null || [[ $(node -v | cut -d'v' -f2 | cut -d'.' -f1) -lt 16 ]]; then
    echo "Установка свежей версии Node.js..."
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt install -y nodejs
fi

echo "========================================="
echo "Установка pipx и Conan"
echo "========================================="

# Установка pipx
python3 -m pip install --user pipx
export PATH="$HOME/.local/bin:$PATH"

# Убеждаемся что pipx в PATH
python3 -m pipx ensurepath

# Установка conan через pipx
pipx install conan

git submodule update --init --recursive

echo "========================================="
echo "Настройка Conan"
echo "========================================="

# Проверка и добавление remote
if ! conan remote list | grep -q "ostis-ai"; then
    conan remote add ostis-ai https://conan.ostis.net/artifactory/api/conan/ostis-ai-library
else
    echo "Remote ostis-ai уже существует"
fi

# Установка профиля с clang
export CC=clang
export CXX=clang++
export CXXFLAGS="-std=c++17"

# Удаляем старый профиль если есть
rm -f ~/.conan2/profiles/default

conan profile detect --force

echo "========================================="
echo "Установка C++ problem solver"
echo "========================================="

./scripts/install_cxx_problem_solver.sh

echo "========================================="
echo "Сборка интерфейса sc-web"
echo "========================================="

cd interface/sc-web

# Установка зависимостей для Ubuntu
if [ -f "scripts/install_deps_ubuntu.sh" ]; then
    ./scripts/install_deps_ubuntu.sh
else
    echo "install_deps_ubuntu.sh not found, using manual installation"
    # Установка зависимостей Python
    pip3 install --user flask flask-cors
    # Установка зависимостей Node.js
    npm install
fi

npm install
npm run build

cd ../..

echo "========================================="
echo "CMake конфигурация и сборка"
echo "========================================="

# Проверяем наличие conan профиля
if [ ! -f ~/.conan2/profiles/default ]; then
    conan profile detect --force
fi

# Устанавливаем зависимости conan
conan install . --build=missing -s compiler=clang -s compiler.version=$(clang --version | head -n1 | grep -oE '[0-9]+' | head -1) -s compiler.libcxx=libstdc++11

# Сборка
cmake --preset release-conan || {
    echo "CMake preset failed, trying manual configuration"
    rm -rf build-release-conan
    cmake --preset release-conan
}

cmake --build --preset release

echo "========================================="
echo "Установка завершена!"
echo "========================================="
echo ""
echo "Дальнейшие действия:"
echo "1. Перезагрузите терминал или выполните: source ~/.bashrc"
echo "2. Собрать базу знаний:   cd industry-demo-ostis && ./scripts/start.sh build_kb"
echo "3. Запустить sc-machine:  ./scripts/start.sh machine"
echo "4. В другом терминале:     ./scripts/start.sh web"
