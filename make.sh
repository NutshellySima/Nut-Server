g++-8 -std=c++17 -Wall -Wextra -Ofast -L /usr/lib ./src/server.cpp ./src/tls.cpp ./src/common.cpp ./src/epollserver.cpp -Iinclude/ -pthread -lstdc++fs -lz -lssl -lcrypto -o server.out
g++-8 -std=c++17 -Wall -Wextra -Ofast ./pages/cgi-bin/calc.cpp -o ./pages/cgi-bin/calc.out
g++-8 -std=c++17 -Wall -Wextra -Ofast ./pages/cgi-bin/ip.cpp -o ./pages/cgi-bin/ip.out
