#include <iostream>
#include <vector>

// Number of positions of base-2 digits to shift when iterating over the long
// space.
void output_array(size_t size, char prefix, const std::string& name) {
  std::cout << "const std::array<std::string, 276>"
            << " " << name << " = {{";
  bool first = true;
  char tag[6];
  for (size_t i = 0; i < size; ++i) {
    if (!first) {
      std::cout << ",\n";
    } else {
      first = false;
    }
    sprintf(tag, "\"%c%04zX\"", prefix, i);
    std::cout << tag;
  }
  std::cout << "}};\n";
}

int main() {
  std::cout << "// Do not modify - auto-generated\n//\n";

  output_array(276, 'T', "kTimerTags");
  output_array(276, 'D', "kDistTags");
}
