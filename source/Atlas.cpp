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

void Atlas::AddBlock(FontChar *item)
{
	m_access.lock();
	m_blocks.push_back(item);
	m_access.unlock();
}

void Atlas::FinishLayout()
{
	auto presort_compare = [](const FontChar* a, const FontChar* b) -> bool
		{
			return a->ch < b->ch;
		};

	auto compare = [](const FontChar* a, const FontChar* b) -> bool
		{
			return a->pb_scaledSDF.h < b->pb_scaledSDF.h;
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
		*pixels++ = 0x00ffffff;
	}

	m_addPageX = 0;
}

bool Atlas::TryAddBlock(FontChar *item)
{
	// got any pages allocated yet?
	if (m_pages.empty())
		return false;

	int w = item->pb_scaledSDF.crop_w;
	int h = item->pb_scaledSDF.crop_h;

	if (w == 0 || h == 0 || w > m_width || h > m_height)
	{
		item->x = 0;
		item->y = 0;
		item->page = 0;
		return true;
	}

	SDL_assert(m_pages.back().m_surface != nullptr);

	// any space for a block...?
	// does it fit horizontally?
	if ((m_addPageX + w) > m_width)
		m_addPageX = 0;

	// find highest column
	int highest = 0;
	for (int i = 0; i < w; i++)
	{
		highest = std::max(highest, m_columnHeights[m_addPageX + i]);
	}

	// does block fit vertically?
	int finalHeight = highest + h;
	if (finalHeight > m_height)
		return false;

	// mark the columns as used
	for (int i = 0; i < w; i++)
	{
		m_columnHeights[m_addPageX + i] = finalHeight+m_padding;
	}

	// ok, copy that block in
	auto dest_surface = m_pages.back().m_surface;
	int dest_pitch = dest_surface->pitch / 4;
	u32* dest_pixels = (u32*)dest_surface->pixels;
	dest_pixels += highest * dest_pitch;

	int src_pitch = w;
	u32* src_pixels = item->pb_scaledSDF.pixels;

	float scalar = item->scaledSize / 512.0f;
	item->x = m_addPageX;
	item->y = highest;
	item->w = w;
	item->h = h;
	item->xoffset = (int)((item->pb_source.crop_x - SDFRange) * scalar);
	item->yoffset = (int)((item->pb_source.h - item->pb_source.crop_y - item->pb_source.crop_h + SDFRange) * scalar);
	item->advance = (int)(item->large_advance * scalar);
		
	for (int yy = 0; yy < h; yy++)
	{
		u32* dest = &dest_pixels[m_addPageX];
		u32* src = &src_pixels[item->pb_scaledSDF.crop_x];
		for (int xx = 0; xx < w; xx++)
		{
			*dest++ = *src++;
		}
		dest_pixels += dest_pitch;
		src_pixels += src_pitch;
	}

	m_addPageX += w + m_padding;

	SDL_assert(m_addPageX >= 0);

	return true;
}



