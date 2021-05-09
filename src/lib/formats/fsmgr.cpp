// license:BSD-3-Clause
// copyright-holders:Olivier Galibert

// Filesystem management code for floppy and hd images

// Currently limited to floppies and creation of preformatted images

#include "emu.h"
#include "fsmgr.h"

void fs_refcounted_inner::ref()
{
	m_ref ++;
}

void fs_refcounted_inner::ref_weak()
{
	m_weak_ref ++;
}

void fs_refcounted_inner::unref()
{
	m_ref --;
	if(m_ref == 0) {
		if(m_weak_ref) {
			drop_weak_references();
			if(m_weak_ref)
				fatalerror("drop_weak_references kept %d active references\n", m_weak_ref);
		} else
			delete this;
	}
}

void fs_refcounted_inner::unref_weak()
{
	m_weak_ref --;
	if(m_weak_ref == 0 && m_ref == 0)
		delete this;
}



void filesystem_manager_t::enumerate_f(floppy_enumerator &fe, uint32_t form_factor, const std::vector<uint32_t> &variants) const
{
}

void filesystem_manager_t::enumerate_h(hd_enumerator &he) const
{
}

void filesystem_manager_t::enumerate_c(cdrom_enumerator &ce) const
{
}

bool filesystem_manager_t::has_variant(const std::vector<uint32_t> &variants, uint32_t variant)
{
	for(uint32_t v : variants)
		if(variant == v)
			return true;
	return false;
}

bool filesystem_manager_t::has(uint32_t form_factor, const std::vector<uint32_t> &variants, uint32_t ff, uint32_t variant)
{
	if(form_factor == floppy_image::FF_UNKNOWN)
		return true;
	if(form_factor != ff)
		return false;
	for(uint32_t v : variants)
		if(variant == v)
			return true;
	return false;
}

std::vector<fs_meta_description> filesystem_manager_t::volume_meta_description() const
{
	std::vector<fs_meta_description> res;
	return res;
}

std::vector<fs_meta_description> filesystem_manager_t::file_meta_description() const
{
	std::vector<fs_meta_description> res;
	return res;
}

std::vector<fs_meta_description> filesystem_manager_t::directory_meta_description() const
{
	std::vector<fs_meta_description> res;
	return res;
}

char filesystem_manager_t::directory_separator() const
{
	return 0; // Subdirectories not supported by default
}

void filesystem_t::format(const fs_meta_data &meta)
{
	fatalerror("format called on a filesystem not supporting it.\n");
}

filesystem_t::dir_t filesystem_t::root()
{
	fatalerror("root called on a filesystem not supporting it.\n");
}

fs_meta_data filesystem_t::metadata()
{
	fatalerror("filesystem_t::metadata called on a filesystem not supporting it.\n");
}

void fsblk_t::set_block_size(uint32_t block_size)
{
	m_block_size = block_size;
}


uint8_t *fsblk_t::iblock_t::offset(const char *function, uint32_t off, uint32_t size)
{
	if(off + size > m_size)
		fatalerror("block_t::%s out-of-block access, offset=%d, size=%d, block size=%d\n", function, off, size, m_size);
	return data() + off;
}

const uint8_t *fsblk_t::iblock_t::rooffset(const char *function, uint32_t off, uint32_t size)
{
	if(off + size > m_size)
		fatalerror("block_t::%s out-of-block read access, offset=%d, size=%d, block size=%d\n", function, off, size, m_size);
	return rodata() + off;
}

void fsblk_t::block_t::copy(u32 offset, const uint8_t *src, u32 size)
{
	uint8_t *blk = m_object->offset("copy", offset, size);
	memcpy(blk, src, size);
}

void fsblk_t::block_t::fill(u32 offset, uint8_t data, u32 size)
{
	uint8_t *blk = m_object->offset("fill", offset, size);
	memset(blk, data, size);
}

void fsblk_t::block_t::fill(uint8_t data)
{
	uint8_t *blk = m_object->data();
	memset(blk, data, m_object->size());
}

void fsblk_t::block_t::wstr(u32 offset, const std::string &str)
{
	uint8_t *blk = m_object->offset("wstr", offset, str.size());
	memcpy(blk, str.data(), str.size());
}

void fsblk_t::block_t::w8(u32 offset, uint8_t data)
{
	uint8_t *blk = m_object->offset("w8", offset, 1);
	blk[0] = data;
}

void fsblk_t::block_t::w16b(u32 offset, u16 data)
{
	uint8_t *blk = m_object->offset("w16b", offset, 2);
	blk[0] = data >> 8;
	blk[1] = data;
}

void fsblk_t::block_t::w24b(u32 offset, u32 data)
{
	uint8_t *blk = m_object->offset("w24b", offset, 3);
	blk[0] = data >> 16;
	blk[1] = data >> 8;
	blk[2] = data;
}

void fsblk_t::block_t::w32b(u32 offset, u32 data)
{
	uint8_t *blk = m_object->offset("w32b", offset, 4);
	blk[0] = data >> 24;
	blk[1] = data >> 16;
	blk[2] = data >> 8;
	blk[3] = data;
}

void fsblk_t::block_t::w16l(u32 offset, u16 data)
{
	uint8_t *blk = m_object->offset("w16l", offset, 2);
	blk[0] = data;
	blk[1] = data >> 8;
}

void fsblk_t::block_t::w24l(u32 offset, u32 data)
{
	uint8_t *blk = m_object->offset("w24l", offset, 3);
	blk[0] = data;
	blk[1] = data >> 8;
	blk[2] = data >> 16;
}

void fsblk_t::block_t::w32l(u32 offset, u32 data)
{
	uint8_t *blk = m_object->offset("w32l", offset, 4);
	blk[0] = data;
	blk[1] = data >> 8;
	blk[2] = data >> 16;
	blk[3] = data >> 24;
}

std::string fsblk_t::block_t::rstr(u32 offset, u32 size)
{
	const u8 *d = m_object->rooffset("rstr", offset, size);
	std::string res;
	for(u32 i=0; i != size; i++)
		res += char(*d++);
	return res;
}

uint8_t fsblk_t::block_t::r8(u32 offset)
{
	const uint8_t *blk = m_object->offset("r8", offset, 1);
	return blk[0];
}

uint16_t fsblk_t::block_t::r16b(u32 offset)
{
	const uint8_t *blk = m_object->offset("r16b", offset, 2);
	return (blk[0] << 8) | blk[1];
}

uint32_t fsblk_t::block_t::r24b(u32 offset)
{
	const uint8_t *blk = m_object->offset("r24b", offset, 3);
	return (blk[0] << 16) | (blk[1] << 8) | blk[2];
}

uint32_t fsblk_t::block_t::r32b(u32 offset)
{
	const uint8_t *blk = m_object->offset("r32b", offset, 4);
	return (blk[0] << 24) | (blk[1] << 16) | (blk[2] << 8) | blk[3];
}

uint16_t fsblk_t::block_t::r16l(u32 offset)
{
	const uint8_t *blk = m_object->offset("r16l", offset, 2);
	return blk[0] | (blk[1] << 8);
}

uint32_t fsblk_t::block_t::r24l(u32 offset)
{
	const uint8_t *blk = m_object->offset("r24l", offset, 3);
	return blk[0] | (blk[1] << 8) | (blk[2] << 16);
}

uint32_t fsblk_t::block_t::r32l(u32 offset)
{
	const uint8_t *blk = m_object->offset("r32l", offset, 4);
	return blk[0] | (blk[1] << 8) | (blk[2] << 16) | (blk[3] << 24);
}

const char *fs_meta_get_name(fs_meta_name name)
{
	switch(name) {
	case fs_meta_name::creation_date: return "creation_date";
	case fs_meta_name::length: return "length";
	case fs_meta_name::loading_address: return "loading_address";
	case fs_meta_name::locked: return "locked";
	case fs_meta_name::sequential: return "sequential";
	case fs_meta_name::modification_date: return "modification_date";
	case fs_meta_name::name: return "name";
	case fs_meta_name::size_in_blocks: return "size_in_blocks";
	case fs_meta_name::os_version: return "os_version";
	case fs_meta_name::os_minimum_version: return "os_minimum_version";
	}
	return "";
}

std::string fs_meta_to_string(fs_meta_type type, const fs_meta &m)
{
	switch(type) {
	case fs_meta_type::string: return m.as_string();
	case fs_meta_type::number: return util::string_format("0x%x", m.as_number());
	case fs_meta_type::flag:   return m.as_flag() ? "t" : "f";
	case fs_meta_type::date:   {
		auto dt = m.as_date();
		return util::string_format("%04d-%02d-%02d %02d:%02d:%02d",
								   dt.year, dt.month, dt.day_of_month,
								   dt.hour, dt.minute, dt.second);
	}
	}
	return std::string("");
}

