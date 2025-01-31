#pragma once

paddr_t alloc_pages(uint32_t);
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);