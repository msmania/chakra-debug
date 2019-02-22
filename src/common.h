#pragma once

typedef uint64_t address_t;

class address_string {
  char buffer_[20];

public:
  address_string(address_t addr);
  operator const char *() const;
};

std::vector<std::string> get_args(const char *args);
uint32_t get_field_offset(const std::string &type, const std::string &field);
address_t load_pointer(address_t addr);
const char *ptos(uint64_t p, char *s, uint32_t len);
void Log(const char* format, ...);

class debug_object {
protected:
  address_t base_{};

public:
  virtual void load(address_t addr) = 0;
  virtual void dump(std::ostream &s) const;
};
