#include "Atlas.h"
#include <algorithm>

void Atlas::StartLayout(int w, int h, int padding)
{
	m_width = w;
	m_height = h;
	m_padding = padding;
	for (auto& page : m_pages)
	{
		if (page.m_texture)
			SDL_DestroyTexture(page.m_texture);
		if (page.m_surface)
			SDL_FreeSurface(page.m_surface);
	}
	m_pages.clear();
	m_blocks.clear();
}

void Atlas::AddBlock(u16 ch, const PixelBlock& block)
{
	m_access.lock();
	auto sdf = new SDF;
	sdf->ch = ch;
	sdf->block = block;
	m_blocks.push_back(sdf);
	m_access.unlock();
}

void Atlas::FinishLayout()
{
	auto compare = [](const SDF* a, const SDF* b) -> bool
		{
			return a->block.crop_h < b->block.crop_h;
		};
	std::sort(m_blocks.begin(), m_blocks.end(), compare);

	for (auto sdf : m_blocks)
	{
		int x, y;
		if (!TryAddBlock(sdf->block, x, y))
		{
			AddNewPage();
			if (!TryAddBlock(sdf->block, x, y))
			{
				SDL_assert(false);
			}
		}
	}

	for (auto& page : m_pages)
	{
		page.m_texture = SDL_CreateTextureFromSurface(m_renderer, page.m_surface);
	}

	m_blocks.clear();
}

void Atlas::AddNewPage()
{
	Page page;
	page.m_surface = SDL_CreateRGBSurfaceWithFormat(0, m_width, m_height, 1, SDL_PIXELFORMAT_ARGB8888);
	SDL_LockSurface(page.m_surface);
	m_pages.push_back(page);

	m_columnHeights.resize(m_width);
	for (int c = 0; c < m_width; c++)
		m_columnHeights[c] = 0;

	// clear the page
	int size = m_width * m_height;
	u32* pixels = (u32*)page.m_surface->pixels;
	while (size--)
	{
		*pixels++ = 0xff000000;
	}

	m_addPageX = 0;
}

bool Atlas::TryAddBlock(const PixelBlock& block, int& x, int& y)
{
	// got any pages allocated yet?
	if (m_pages.empty())
		return false;

	if (block.crop_w == 0 || block.crop_h == 0)
	{
		x = 0;
		y = 0;
		return true;
	}

	SDL_assert(m_pages.back().m_surface != nullptr);

	// does block even fit in an empty page 
	if (block.crop_w > m_width || block.crop_h > m_height)
		return false;

	// any space for a block...?
	// does it fit horizontally?
	if ((m_addPageX + block.crop_w) > m_width)
		m_addPageX = 0;

	// find highest column
	int highest = 0;
	for (int i = 0; i < block.crop_w; i++)
	{
		highest = std::max(highest, m_columnHeights[m_addPageX + i]);
	}

	// does block fit vertically?
	int finalHeight = highest + block.crop_h;
	if (finalHeight > m_height)
		return false;

	// mark the columns as used
	for (int i = 0; i < block.crop_w; i++)
	{
		m_columnHeights[m_addPageX + i] = finalHeight+m_padding;
	}

	// ok, copy that block in
	auto dest_surface = m_pages.back().m_surface;
	int dest_pitch = dest_surface->pitch / 4;
	u32* dest_pixels = (u32*)dest_surface->pixels;
	dest_pixels += highest * dest_pitch;

	int src_pitch = block.pitch / 4;
	u32* src_pixels = block.pixels;
	src_pixels += block.crop_y * src_pitch;

	x = m_addPageX;
	y = highest;

	for (int yy = 0; yy < block.crop_h; yy++)
	{
		u32* dest = &dest_pixels[m_addPageX];
		u32* src = &src_pixels[block.crop_x];
		for (int xx = 0; xx < block.crop_w; xx++)
		{
			*dest++ = *src++;
		}
		dest_pixels += dest_pitch;
		src_pixels += src_pitch;
	}

	SDL_assert(block.crop_w > 0);

	m_addPageX += block.crop_w + m_padding;

	SDL_assert(m_addPageX >= 0);

	return true;
}



