#!/bin/bash

echo "========================================="
echo "Установка system dependencies (быстрая)"
echo "========================================="

# Установка всех пакетов через dnf (включая cmake)
sudo dnf install -y \
    curl \
    ccache \
    python3 \
    python3-pip \
    python3-virtualenv \
    gcc \
    gcc-c++ \
    make \
    ninja-build \
    nodejs \
    npm \
    clang \
    git \
    cmake 

# Установка pipx (без cmake!)
python3 -m pip install --user pipx
export PATH="$HOME/.local/bin:$PATH"

# Устанавливаем только conan через pipx
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

# Установка зависимостей
./scripts/install_deps_npm.sh || {
    echo "install_deps_npm.sh failed, trying manual npm install"
    npm install
}

./scripts/install_deps_python.sh || {
    echo "install_deps_python.sh failed, trying manual pip install"
    pip3 install -r requirements.txt
}

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
conan install . --build=missing -s compiler=clang -s compiler.version=19 -s compiler.libcxx=libstdc++11

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
