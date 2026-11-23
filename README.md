# корень сборки
cmake -S . -B build
# пересборка
cmake --build build
# запуск
./build/bin/vtsh
./build/bin/vtsh < commands.txt

# Полная пересборка
rm -rf build
cmake -S . -B build
cmake --build build

# Запуск тестов 
python3 -m unittest discover -s test -v
