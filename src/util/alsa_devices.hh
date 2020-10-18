#include <string>
#include <vector>

class ALSADevices
{
public:
  struct Device
  {
    std::string name;
    std::vector<std::pair<std::string, std::string>> outputs;
  };

  std::vector<Device> list();
};
