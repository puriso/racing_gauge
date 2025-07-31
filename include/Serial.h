#ifndef SERIAL_H
#define SERIAL_H
class SerialClass
{
 public:
  void begin(int) {}
  template <typename... Args>
  void printf(const char*, Args...)
  {
  }
  void println(const char*) {}
};
inline SerialClass Serial;
#endif  // SERIAL_H
