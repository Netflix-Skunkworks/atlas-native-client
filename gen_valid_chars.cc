#include <array>
#include <fstream>

void dump_array(std::ostream& os, const std::string& name,
                const std::array<bool, 128>& chars) {
  os << "const std::array<bool, 128> " << name << " = {{";
  os.setf(std::ios::boolalpha);

  os << chars[0];
  for (int i = 1; i < chars.size(); ++i) {
    os << ", " << chars[i];
  }

  os << "}};\n";
}

int main(int argc, char* argv[]) {
  std::ofstream of;
  if (argc > 1) {
    of.open(argv[1]);
  } else {
    of.open("/dev/stdout");
  }

  std::array<bool, 128> charsAllowed;
  for (int i = 0; i < 128; ++i) {
    charsAllowed[i] = false;
  }

  charsAllowed['.'] = true;
  charsAllowed['-'] = true;
  charsAllowed['_'] = true;

  for (auto ch = '0'; ch <= '9'; ++ch) {
    charsAllowed[ch] = true;
  }
  for (auto ch = 'a'; ch <= 'z'; ++ch) {
    charsAllowed[ch] = true;
  }
  for (auto ch = 'A'; ch <= 'Z'; ++ch) {
    charsAllowed[ch] = true;
  }

  dump_array(of, "kCharsAllowed", charsAllowed);

  charsAllowed['~'] = true;
  charsAllowed['^'] = true;
  dump_array(of, "kGroupCharsAllowed", charsAllowed);
};
