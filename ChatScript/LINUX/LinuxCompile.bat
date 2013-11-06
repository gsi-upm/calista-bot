g++ -lpthread -lrt -funsigned-char src/*.cpp -O2 -oLinuxChatScript 2>err.txt
# no improvement is seen using -O3.

# rm -f *.* to get no confirm requests














for xmpp (not in this product):
g++ -lpthread -lrt -lresolv -DXMPP *.c *.cpp -O2 -o XMPP.out
