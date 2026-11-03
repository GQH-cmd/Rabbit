#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "GatewayValidation.h"

using esp_err_t = int;
using nvs_handle_t = unsigned;
constexpr esp_err_t ESP_OK=0, ESP_FAIL=-1, ESP_ERR_INVALID_SIZE=0x104;
constexpr esp_err_t ESP_ERR_NVS_NOT_INITIALIZED=0x1101;
constexpr esp_err_t ESP_ERR_NVS_NOT_ENOUGH_SPACE=0x1105;
constexpr esp_err_t ESP_ERR_NVS_INVALID_LENGTH=0x110c;
constexpr esp_err_t ESP_ERR_FLASH_OP_FAIL=0x106;
constexpr int NVS_READWRITE=1;
struct nvs_stats_t {
  size_t used_entries,free_entries,available_entries,total_entries,namespace_count;
};
static const char* esp_err_to_name(esp_err_t e){
  switch(e){
    case ESP_OK:return "ESP_OK";case ESP_FAIL:return "ESP_FAIL";
    case ESP_ERR_INVALID_SIZE:return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_NVS_NOT_INITIALIZED:return "ESP_ERR_NVS_NOT_INITIALIZED";
    case ESP_ERR_NVS_NOT_ENOUGH_SPACE:return "ESP_ERR_NVS_NOT_ENOUGH_SPACE";
    case ESP_ERR_NVS_INVALID_LENGTH:return "ESP_ERR_NVS_INVALID_LENGTH";
    case ESP_ERR_FLASH_OP_FAIL:return "ESP_ERR_FLASH_OP_FAIL";
    default:return "ESP_ERR_UNKNOWN";
  }
}
struct SerialCapture {
  std::string output;
  void printf(const char* format,...){
    char buffer[1024];va_list args;va_start(args,format);
    std::vsnprintf(buffer,sizeof buffer,format,args);va_end(args);output+=buffer;
  }
} Serial;
struct FakeNvs {
  esp_err_t openError=ESP_OK,writeError=ESP_OK,commitError=ESP_OK,readError=ESP_OK,statsError=ESP_OK;
  unsigned opens=0,writes=0,commits=0,reads=0,closes=0,stats=0;
  bool shortRead=false,corrupt=false,handleOpen=false;
  std::string name,key;
  std::vector<unsigned char> stored;
} fake;
static esp_err_t nvs_open(const char* name,int mode,nvs_handle_t* h){
  ++fake.opens;fake.name=name;
  if(mode!=NVS_READWRITE)return ESP_FAIL;
  if(fake.openError)return fake.openError;
  fake.handleOpen=true;*h=17;return ESP_OK;
}
static esp_err_t nvs_set_blob(nvs_handle_t h,const char* key,const void* data,size_t size){
  ++fake.writes;fake.key=key;
  if(h!=17||!fake.handleOpen)return ESP_FAIL;
  if(fake.writeError)return fake.writeError;
  const auto* p=static_cast<const unsigned char*>(data);fake.stored.assign(p,p+size);return ESP_OK;
}
static esp_err_t nvs_commit(nvs_handle_t h){
  ++fake.commits;if(h!=17||!fake.handleOpen)return ESP_FAIL;return fake.commitError;
}
static esp_err_t nvs_get_blob(nvs_handle_t h,const char* key,void* out,size_t* size){
  ++fake.reads;
  if(h!=17||!fake.handleOpen||std::strcmp(key,"config"))return ESP_FAIL;
  if(fake.readError){*size=fake.stored.size()+1;return fake.readError;}
  if(!out){*size=fake.stored.size();return ESP_OK;}
  if(*size<fake.stored.size()){*size=fake.stored.size();return ESP_ERR_NVS_INVALID_LENGTH;}
  *size=fake.stored.size()-(fake.shortRead?1:0);
  std::memcpy(out,fake.stored.data(),*size);
  if(fake.corrupt&&*size)static_cast<unsigned char*>(out)[0]^=1;
  return ESP_OK;
}
static void nvs_close(nvs_handle_t h){if(h==17&&fake.handleOpen){++fake.closes;fake.handleOpen=false;}}
static esp_err_t nvs_get_stats(const char* partition,nvs_stats_t* stats){
  ++fake.stats;if(partition)return ESP_FAIL;if(fake.statsError)return fake.statsError;
  *stats={5,625,499,630,2};return ESP_OK;
}
// Compatibility adapter lets the unmodified production function run for RED.
// Both implementations hit the same fake NVS backend, rather than testing a copied algorithm.
struct Preferences {
  nvs_handle_t h=0;
  bool begin(const char* name,bool ro){return !ro&&nvs_open(name,NVS_READWRITE,&h)==ESP_OK;}
  size_t putBytes(const char* key,const void* value,size_t size){
    return nvs_set_blob(h,key,value,size)==ESP_OK&&nvs_commit(h)==ESP_OK?size:0;
  }
  size_t getBytes(const char* key,void* value,size_t size){return nvs_get_blob(h,key,value,&size)==ESP_OK?size:0;}
  void end(){nvs_close(h);}
};
#include "network_save_extracted.h"

static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\nLOG=%s\n",__LINE__,#x,Serial.output.c_str());std::exit(1);}}while(0)
static gateway::Config config(){
  gateway::Config c{};std::strcpy(c.ssid,"PRIVATE_SSID_MARKER");
  std::strcpy(c.wifiPassword,"PRIVATE_WIFI_PASSWORD_MARKER");
  std::strcpy(c.host,"private-broker-marker.local");
  std::strcpy(c.username,"PRIVATE_USERNAME_MARKER");
  std::strcpy(c.password,"PRIVATE_MQTT_PASSWORD_MARKER");
  std::strcpy(c.device,"PRIVATE_DEVICE_MARKER");gateway::seal(c);return c;
}
static void reset(){fake=FakeNvs{};Serial.output.clear();}
static void privateValuesAbsent(const gateway::Config& c){
  for(const char* s:{c.ssid,c.wifiPassword,c.host,c.username,c.password,c.device})CHECK(Serial.output.find(s)==std::string::npos);
}
static void stage(const char* wanted,const char* error,const gateway::Config& c){
  CHECK(Serial.output.find(std::string("NET_SAVE_FAIL stage=")+wanted)!=std::string::npos);
  CHECK(Serial.output.find(error)!=std::string::npos);CHECK(fake.stats==1);privateValuesAbsent(c);
}
int main(){
  auto c=config();CHECK(gateway::validConfig(c));
  reset();CHECK(persistConfig(c));CHECK(fake.opens==1&&fake.writes==1&&fake.commits==1&&fake.reads==1&&fake.closes==1);
  CHECK(fake.name=="rabbitnet"&&fake.key=="config");CHECK(fake.stored.size()==sizeof c);
  CHECK(!std::memcmp(fake.stored.data(),&c,sizeof c));CHECK(Serial.output.find("NET_SAVE_OK")!=std::string::npos);privateValuesAbsent(c);
  reset();fake.openError=ESP_ERR_NVS_NOT_INITIALIZED;CHECK(!persistConfig(c));stage("open","ESP_ERR_NVS_NOT_INITIALIZED",c);
  CHECK(!fake.closes&&!fake.writes&&!fake.commits&&!fake.reads);
  reset();fake.writeError=ESP_ERR_NVS_NOT_ENOUGH_SPACE;CHECK(!persistConfig(c));stage("write","ESP_ERR_NVS_NOT_ENOUGH_SPACE",c);
  CHECK(fake.closes==1&&!fake.commits&&!fake.reads);
  reset();fake.commitError=ESP_ERR_FLASH_OP_FAIL;CHECK(!persistConfig(c));stage("commit","ESP_ERR_FLASH_OP_FAIL",c);
  CHECK(fake.closes==1&&!fake.reads);
  reset();fake.readError=ESP_ERR_NVS_INVALID_LENGTH;CHECK(!persistConfig(c));stage("readback","ESP_ERR_NVS_INVALID_LENGTH",c);
  CHECK(fake.closes==1&&fake.reads==1);
  reset();fake.shortRead=true;CHECK(!persistConfig(c));stage("verify_length","ESP_ERR_INVALID_SIZE",c);CHECK(fake.closes==1);
  reset();fake.corrupt=true;CHECK(!persistConfig(c));stage("verify_mismatch","ESP_FAIL",c);CHECK(fake.closes==1);
  reset();fake.openError=ESP_ERR_NVS_NOT_INITIALIZED;fake.statsError=ESP_FAIL;CHECK(!persistConfig(c));stage("open","ESP_ERR_NVS_NOT_INITIALIZED",c);
  CHECK(Serial.output.find("NET_SAVE_NVS stats_error=")!=std::string::npos);
  reset();fake.writeError=ESP_ERR_NVS_NOT_ENOUGH_SPACE;CHECK(!persistConfig(c));
  CHECK(Serial.output.find("available=499")!=std::string::npos);CHECK(Serial.output.find("free=625")!=std::string::npos);
  std::printf("PASS %u network save diagnostic checks (real production function; no hardware)\n",checks);
}
