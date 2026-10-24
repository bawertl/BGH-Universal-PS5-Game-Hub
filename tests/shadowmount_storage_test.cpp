#include "storage_shadowmount.hpp"
#include "storage_widgets.hpp"
#include <cassert>
#include <string>
using ulg::shadow_storage::Snapshot;
using ulg::shadow_storage::parse_storage_json;
using ulg::shadow_storage::decode_http;
static const char* kMounted=R"({"status":0,"count":4,"destination_count":2,"mounts":[
{"source":"/dev/ssd","mount_point":"/user","filesystem":"BFS","total_bytes":667000000000,"free_bytes":250000000000,"available_bytes":240000000000,"used_bytes":427000000000,"read_only":false},
{"source":"/dev/usbdisk-1","mount_point":"/mnt/usb0","filesystem":"EXFATFS","total_bytes":2000000000000,"free_bytes":1000000000000,"available_bytes":990000000000,"used_bytes":1010000000000,"read_only":false},
{"source":"/dev/usbdisk-1","mount_point":"/mnt/usb1","filesystem":"EXFATFS","total_bytes":2000000000000,"free_bytes":1000000000000,"available_bytes":990000000000,"used_bytes":1010000000000,"read_only":false},
{"source":"/dev/usbdisk-2","mount_point":"/mnt/ext0","filesystem":"EXFATFS","total_bytes":1000000000000,"free_bytes":700000000000,"available_bytes":690000000000,"used_bytes":310000000000,"read_only":false}],
"destinations":[{"path":"/data/homebrew","total_bytes":999999999999999999}]})";
int main(){
  Snapshot r;assert(parse_storage_json(kMounted,r));
  assert(r.http_ok && r.internal.valid && r.internal.total==667000000000ULL);
  assert(r.internal.available==240000000000ULL);
  assert(r.usb.valid && r.usb_count==2 && r.usb.total==3000000000000ULL);
  assert(r.usb.used==1320000000000ULL && r.usb.available==1680000000000ULL);
  assert(ulg::storage_widgets::format_gib(2000000000000ULL)=="2.0 TB");
  assert(ulg::storage_widgets::format_gib(350000000000ULL)=="350 GB");
  const std::string input=kMounted;
  for(const auto &bad:{std::string("{}"),std::string("{\"status\":1,\"mounts\":[]}"),
      std::string("{\"status\":0,\"mounts\": [ {\"mount_point\":\"/user\",\"total_bytes\":1} ]}"),
      std::string("{\"status\":0,\"mounts\":[}"),
      std::string("{\"status\":0,\"mounts\":[]")+"INVALID"}){
    Snapshot previous=r;
    assert(!parse_storage_json(bad,r));
    assert(r.internal.total==previous.internal.total);
  }
  assert(!parse_storage_json(input.substr(0,input.size()-8),r));
  Snapshot noUsb;
  assert(parse_storage_json(R"({"mounts":[],"status":0,"destinations":[],"count":0})",noUsb));
  assert(noUsb.http_ok && !noUsb.usb.valid && !noUsb.internal.valid);
  std::string json;
  const std::string simple="{\"mounts\":[],\"status\":0}";
  const std::string http="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 25\r\n\r\n"+simple;
  // Content length must exactly match, including no invisible trailing bytes.
  assert(!decode_http(http,json));
  const std::string correct="HTTP/1.1 200 OK\r\nContent-Length: "+std::to_string(simple.size())+"\r\n\r\n"+simple;
  assert(decode_http(correct,json)&&json==simple);
  assert(!decode_http("HTTP/1.1 404 Not Found\r\n\r\n{}",json));
  assert(!decode_http("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",json));
}
