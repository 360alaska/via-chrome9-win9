/* VX855 cache-attribute report (v10.68): read-only. Answers why CPU writes to the framebuffer are slow: which cache type the CPU
 * really uses for the video memory. Reports the CPUID signature, the MTRR and PAT registers, the page-table bits of the
 * framebuffer mapping, and what the Windows MTRR service answered when the driver asked for write-combining.
 * Nothing here changes any state. MSRs are read only after CPUID says they exist. */
#include "winhack.h"
#include "vmm.h"
#include "vxd_lib.h"
#include "vx85598.h"
#include "mtrr.h"

unsigned long vx_mtrr_called, vx_mtrr_ver, vx_mtrr_status, vx_mtrr_base, vx_mtrr_size, vx_mtrr_conf;

/* CPUID present? (EFLAGS bit 21 can be toggled) */
unsigned long vxc_has_cpuid(void);
#pragma aux vxc_has_cpuid = 0x9C 0x58 0x89 0xC1 0x35 0x00 0x00 0x20 0x00 0x50 0x9D 0x9C 0x58 0x31 0xC8 0x25 0x00 0x00 0x20 0x00 0x51 0x9D value [eax] modify exact [eax ecx];
/* CPUID leaf eax -> out[0..3] = eax ebx ecx edx; ebx is saved around it */
void vxc_cpuid(unsigned long leaf, unsigned long *out);
#pragma aux vxc_cpuid = 0x53 0x0F 0xA2 0x89 0x06 0x89 0x5E 0x04 0x89 0x4E 0x08 0x89 0x56 0x0C 0x5B parm [eax] [esi] modify exact [eax ecx edx];
/* RDMSR ecx -> out[0]=eax(lo) out[1]=edx(hi) */
void vxc_rdmsr(unsigned long msr, unsigned long *out);
#pragma aux vxc_rdmsr = 0x0F 0x32 0x89 0x06 0x89 0x56 0x04 parm [ecx] [esi] modify exact [eax edx];

void VX_cache_report(vx_cache_report *r) {
 unsigned long q[4], i, n;
 unsigned char *z=(unsigned char*)r;
 for(i=0;i<sizeof(*r);i++) z[i]=0;
 r->magic=VX_MAGIC; r->abi=VX_CACHE_ABI;
 r->mtrr_called=vx_mtrr_called; r->mtrr_ver=vx_mtrr_ver; r->mtrr_status=vx_mtrr_status;
 r->mtrr_base=vx_mtrr_base; r->mtrr_size=vx_mtrr_size; r->mtrr_conf=vx_mtrr_conf;
 r->fb_phys=vx_status.fb_phys; r->fb_linear=vx_status.fb_linear; r->fb_map_bytes=vx_map_bytes;
 r->has_cpuid=vxc_has_cpuid()?1:0;
 if(r->has_cpuid) {
  vxc_cpuid(0,q); r->cpuid_max=q[0]; r->vendor[0]=q[1]; r->vendor[1]=q[3]; r->vendor[2]=q[2]; /* "Cent" "aurH" "auls" order: ebx edx ecx */
  if(q[0]>=1) {vxc_cpuid(1,q); r->sig=q[0]; r->feat_ecx=q[2]; r->feat_edx=q[3];}
  vxc_cpuid(0x80000000UL,q); r->ext_max=q[0];
  /* MSR (bit 5) and MTRR (bit 12) present: read the cache-control registers */
  if((r->feat_edx&(1UL<<5)) && (r->feat_edx&(1UL<<12))) {
   vxc_rdmsr(0xFEUL,q); r->mtrrcap[0]=q[0]; r->mtrrcap[1]=q[1];
   vxc_rdmsr(0x2FFUL,q); r->deftype[0]=q[0]; r->deftype[1]=q[1];
   n=r->mtrrcap[0]&0xFF; if(n>16) n=16; r->var_count=n;
   for(i=0;i<n;i++) {
    vxc_rdmsr(0x200UL+2*i,q);   r->var[i*4+0]=q[0]; r->var[i*4+1]=q[1];
    vxc_rdmsr(0x200UL+2*i+1,q); r->var[i*4+2]=q[0]; r->var[i*4+3]=q[1];
   }
   if(r->feat_edx&(1UL<<16)) {vxc_rdmsr(0x277UL,q); r->pat[0]=q[0]; r->pat[1]=q[1];}
   r->msr_ok=1;
  }
 }
 /* page-table bits of the framebuffer mapping: first page, 1 MB in, 8 MB in, last mapped page */
 if(vx_status.fb_linear && vx_map_bytes>=4096UL) {
  unsigned long pages=vx_map_bytes>>12, pg[4], base=vx_status.fb_linear>>12, k;
  pg[0]=0; pg[1]=256; pg[2]=2048; pg[3]=pages-1;
  for(k=0;k<4;k++) {
   unsigned long e=0;
   if(pg[k]>=pages) continue;
   if(k && pg[k]==pg[k-1]) continue;
   _CopyPageTable(base+pg[k],1,(DWORD*)&e,0);
   r->pte_page[r->pte_count]=pg[k]; r->pte[r->pte_count]=e; r->pte_count++;
  }
 }
}

/* Diagnostic (VX2READ): temporarily change the cache type of the framebuffer MTRR range through the same Windows service the driver
 * used at start, to measure CPU reads with another type. Type 2 is the driver's own setting and is put back by VX_cache_restore
 * (also called when a handle to the driver closes). Returns the service status, or FFFFFFFF when the driver never set the range. */
static unsigned long vx_mtrr_test_active;
unsigned long VX_cache_set(unsigned long type) {
 unsigned long st;
 if(!vx_mtrr_called || !vx_mtrr_size || type>3UL) return 0xFFFFFFFFUL;
 st=MTRR_SetPhysicalCacheTypeRange(vx_mtrr_base,0,vx_mtrr_size,type);
 if(!st) vx_mtrr_test_active=(type!=2UL); /* v10.330 (review #51): only when the service changed the type */
 return st;
}
void VX_cache_restore(void) {
 if(vx_mtrr_test_active) VX_cache_set(2UL);
}
/* v10.283: the memory type the variable MTRRs give a physical address (0 UC, 1 WC, 4 WT, 5 WP, 6 WB; UC wins over the others, WT over
 * WB; no range: the default type), 0xFF when the MTRRs cannot be read. Read-only. */
unsigned long VX_cache_type_at(unsigned long phys) {
 unsigned long q[4],cap,def,n,i,t=0xFFFFUL,b[2],m[2];
 if(!vxc_has_cpuid()) return 0xFFUL;
 vxc_cpuid(0,q); if(q[0]<1) return 0xFFUL;
 vxc_cpuid(1,q); if(!(q[3]&(1UL<<5))||!(q[3]&(1UL<<12))) return 0xFFUL;
 vxc_rdmsr(0xFEUL,q); cap=q[0]; vxc_rdmsr(0x2FFUL,q); def=q[0];
 if(!(def&0x800UL)) return 0; /* MTRRs off: everything UC */
 n=cap&0xFF; if(n>16) n=16;
 for(i=0;i<n;i++) {
  vxc_rdmsr(0x200UL+2*i,b); vxc_rdmsr(0x200UL+2*i+1,m);
  if(!(m[0]&0x800UL)) continue;
  if((phys&m[0]&0xFFFFF000UL)!=(b[0]&m[0]&0xFFFFF000UL)) continue;
  if((b[0]&0xFF)==0) return 0;
  if(t==0xFFFFUL||((b[0]&0xFF)==4&&t==6)) t=b[0]&0xFF;
 }
 return t==0xFFFFUL?(def&0xFF):t;
}
