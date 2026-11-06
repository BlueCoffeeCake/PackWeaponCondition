// Decode and toggle hooks only in private, non-executed image copies.
#include "PackWeaponCondition.cpp"
static void dummy(){}
static void* image(const char* path){
 FILE* f=fopen(path,"rb");if(!f)return nullptr;fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);auto b=(uint8_t*)malloc(size);if(fread(b,1,size,f)!=size)return nullptr;fclose(f);
 auto dos=(IMAGE_DOS_HEADER*)b;auto pe=(IMAGE_NT_HEADERS64*)(b+dos->e_lfanew);auto p=(uint8_t*)VirtualAlloc(nullptr,pe->OptionalHeader.SizeOfImage,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
 if(!p)return nullptr;memcpy(p,b,pe->OptionalHeader.SizeOfHeaders);auto section=IMAGE_FIRST_SECTION(pe);
 for(unsigned i=0;i<pe->FileHeader.NumberOfSections;i++)if(section[i].SizeOfRawData)memcpy(p+section[i].VirtualAddress,b+section[i].PointerToRawData,section[i].SizeOfRawData);
 free(b);return p;
}
int main(int argc,char** argv){
 if(argc!=3)return 2;auto c=(uint8_t*)image(argv[1]),g=(uint8_t*)image(argv[2]);if(!c||!g)return 3;
 uintptr_t offsets[]={R_Flags,R_Cap,R_Cost,R_Roll,R_Wake,R_Inventory,R_World,R_Pickup,R_Input,R_Equip,R_Settings,R_Wear,R_Card,R_Depleted,G_QueueFire,G_Fire};
 const unsigned char* bytes[]={B_Flags,B_Cap,B_Cost,B_Roll,B_Wake,B_Inventory,B_World,B_Pickup,B_Input,B_Equip,B_Settings,B_Wear,B_Card,B_Depleted,E_QueueFire,E_Fire};
 if(MH_Initialize()!=MH_OK)return 4;
 for(unsigned i=0;i<16;i++){
  void* address=(i<14?c:g)+offsets[i];void* original=nullptr;
  if(memcmp(address,bytes[i],16))return 10+i;
  auto result=MH_CreateHook(address,(void*)dummy,&original);if(result!=MH_OK){printf("Hook %u failed: %d\n",i,result);return 50+i;}
 }
 if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK)return 70;if(MH_DisableHook(MH_ALL_HOOKS)!=MH_OK)return 71;
 for(unsigned i=0;i<16;i++)if(memcmp((i<14?c:g)+offsets[i],bytes[i],16))return 80+i;
 if(MH_Uninitialize()!=MH_OK)return 100;
 VirtualFree(c,0,MEM_RELEASE);VirtualFree(g,0,MEM_RELEASE);
 puts("PASS 16 exact native hook sites: decoded, enabled and restored on private non-executed image copies. No game process.");return 0;
}
