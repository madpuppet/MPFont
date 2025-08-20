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
			SDL_DestroySurface(page.m_surface);
	}
	m_pages.clear();
	m_blocks.clear();
}

void Atlas::AddBlock(FontChar *item)
{
	m_access.lock();
	m_blocks.push_back(item);
	m_access.unlock();
}

void Atlas::LayoutBlocks()
{
	auto presort_compare = [](const FontChar* a, const FontChar* b) -> bool
		{
			return a->ch < b->ch;
		};

	auto compare = [](const FontChar* a, const FontChar* b) -> bool
		{
			return a->pb_scaledSDF.crop_h < b->pb_scaledSDF.crop_h;
		};
	std::sort(m_blocks.begin(), m_blocks.end(), presort_compare);
	std::sort(m_blocks.begin(), m_blocks.end(), compare);

	for (auto item : m_blocks)
	{
		if (!TryAddBlock(item))
		{
			AddNewPage();
			if (!TryAddBlock(item))
			{
				SDL_assert(false);
			}
		}
	}
}

void Atlas::CreatePageTextures()
{
	for (auto& page : m_pages)
	{
		page.m_texture = SDL_CreateTextureFromSurface(m_renderer, page.m_surface);
	}
	m_blocks.clear();
}

void Atlas::AddNewPage()
{
	Page page;
	page.m_surface = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_ARGB8888);
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
		*pixels++ = 0x00ffffff;
	}

	m_addPageX = 0;
}

bool Atlas::TryAddBlock(FontChar *item)
{
	// got any pages allocated yet?
	if (m_pages.empty())
		return false;

	if (item->w == 0 || item->h == 0 || item->w > m_width || item->h > m_height)
	{
		item->x = 0;
		item->y = 0;
		item->page = 0;
		return true;
	}

	SDL_assert(m_pages.back().m_surface != nullptr);

	// any space for a block...?
	// does it fit horizontally?
	if ((m_addPageX + item->w) > m_width)
		m_addPageX = 0;

	// find highest column
	int highest = 0;
	for (int i = 0; i < item->w+m_padding && (i + m_addPageX) < m_width; i++)
	{
		highest = std::max(highest, m_columnHeights[m_addPageX + i]);
	}

	// does block fit vertically?
	int finalHeight = highest + item->h;
	if (finalHeight > m_height)
		return false;

	// mark the columns as used
	for (int i = 0; i < item->w; i++)
	{
		m_columnHeights[m_addPageX + i] = finalHeight+m_padding;
	}

	// ok, copy that block in
	auto dest_surface = m_pages.back().m_surface;
	int dest_pitch = dest_surface->pitch / 4;
	u32* dest_pixels = (u32*)dest_surface->pixels;
	dest_pixels += highest * dest_pitch;

	int src_pitch = item->pb_scaledSDF.pitch/4;
	u32* src_pixels = &item->pb_scaledSDF.pixels[item->pb_scaledSDF.crop_y * src_pitch];

	item->x = m_addPageX;
	item->y = highest;
		
	for (int yy = 0; yy < item->h; yy++)
	{
		u32* dest = &dest_pixels[m_addPageX];
		u32* src = &src_pixels[item->pb_scaledSDF.crop_x];
		for (int xx = 0; xx < item->w; xx++)
		{
			*dest++ = *src++;
		}
		dest_pixels += dest_pitch;
		src_pixels += src_pitch;
	}

	m_addPageX += item->w + m_padding;

	SDL_assert(m_addPageX >= 0);

	return true;
}



