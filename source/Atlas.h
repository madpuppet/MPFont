#pragma once

#include "types.h"
#include <mutex>
#include <vector>
#include <SDL.h>
#include "SDL_Surface.h"
#include "PixelBlock.h"

struct PixelBlock;

struct LayoutPos
{
	int ch;
	int x;
	int y;
};

class Atlas
{
public:
	struct SDF
	{
		PixelBlock block;
		u16 ch;
	};
	struct Page
	{
		SDL_Surface* m_surface = nullptr;
		SDL_Texture* m_texture = nullptr;
	};

	void SetRenderer(SDL_Renderer* renderer) { m_renderer = renderer; }
	void StartLayout(int w, int h, int padding);
	void AddBlock(u16 ch, const PixelBlock &block);
	void FinishLayout(std::vector<LayoutPos> &layoutPos);
	std::vector<Page>& Pages() { return m_pages; }

private:
	bool TryAddBlock(const PixelBlock &block, int &x, int &y);
	void AddNewPage();

	SDL_Renderer* m_renderer = nullptr;

	std::mutex m_access;

	int m_width = 0;
	int m_height = 0;
	int m_padding = 1;
	
	std::vector<Page> m_pages;
	int m_addPageX = 0;

	// blocks queued for layout before being sorted
	std::vector<SDF*> m_blocks;
	// current height of each column
	std::vector<int> m_columnHeights;
};

