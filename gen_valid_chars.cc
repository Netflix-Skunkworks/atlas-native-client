#include <array>
#include <iostream>

void dump_array(const std::string& name, const std::array<bool, 128>& chars) {
  using std::cout;
  cout << "const std::array<bool, 128> " << name << " = {{";
  cout.setf(std::ios::boolalpha);

  cout << chars[0];
  for (int i = 1; i < chars.size(); ++i) {
    cout << ", " << chars[i];
  }

  cout << "}};\n";
}

int main() {
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

  dump_array("kCharsAllowed", charsAllowed);

  charsAllowed['~'] = true;
  charsAllowed['^'] = true;
  dump_array("kGroupCharsAllowed", charsAllowed);
};
