/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "core/core.h"
#include "core/version.h"
#include "util/socket.h"

#include <SDL.h>

#define DEFAULT_PORT 13721

int main() {
	SocketSubsystemInit();
	struct Address serverIP = {
		.version = IPV4,
		.ipv4 = 0x7F000001
	};
	Socket server = SocketConnectTCP(DEFAULT_PORT, &serverIP);
	if (SOCKET_FAILED(server)) {
		SocketSubsystemDeinit();
		return 1;
	}

	unsigned width, height, bpp;
	SocketRecv(server, &width, sizeof(width));
	SocketRecv(server, &height, sizeof(height));
	SocketRecv(server, &bpp, sizeof(bpp));
	width = ntohl(width);
	height = ntohl(height);
	if (ntohl(bpp) != BYTES_PER_PIXEL) {
		SocketClose(server);
		SocketSubsystemDeinit();
		return 1;
	}
	ssize_t bufferSize = width * height * BYTES_PER_PIXEL;

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(width, height, 16, SDL_DOUBLEBUF | SDL_HWSURFACE);
#else
	SDL_SetVideoMode(width, height, 32, SDL_DOUBLEBUF | SDL_HWSURFACE);
#endif
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window = SDL_CreateWindow(projectName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	Uint32 pixfmt;
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	pixfmt = SDL_PIXELFORMAT_RGB565;
#else
	pixfmt = SDL_PIXELFORMAT_ABGR1555;
#endif
#else
	pixfmt = SDL_PIXELFORMAT_ABGR8888;
#endif
	SDL_Texture* sdlTex = SDL_CreateTexture(renderer, pixfmt, SDL_TEXTUREACCESS_STREAMING, width, height);
#endif


	SDL_Event event;
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Surface* surface = SDL_GetVideoSurface();
#endif

	int keys = 0;
	while (true) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				SocketClose(server);
				break;
			}
			if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) {
				continue;
			}
			int key = 0;
			switch (event.key.keysym.sym) {
			case SDLK_x:
				key = 1;
				break;
			case SDLK_z:
				key = 2;
				break;
			case SDLK_BACKSPACE:
				key = 4;
				break;
			case SDLK_RETURN:
				key = 8;
				break;
			case SDLK_RIGHT:
				key = 16;
				break;
			case SDLK_LEFT:
				key = 32;
				break;
			case SDLK_UP:
				key = 64;
				break;
			case SDLK_DOWN:
				key = 128;
				break;
			case SDLK_s:
				key = 256;
				break;
			case SDLK_a:
				key = 512;
				break;
			default:
				break;
			}
			if (event.type == SDL_KEYDOWN) {
				keys |= key;
			} else {
				keys &= ~key;
			}
		}
		uint16_t keysNO = htons(keys);
		if (SocketSend(server, &keysNO, sizeof(keysNO)) != sizeof(keysNO)) {
			break;
		}
		void* pixels;
#if SDL_VERSION_ATLEAST(2, 0, 0)
		int pitch;
		SDL_LockTexture(sdlTex, NULL, &pixels, &pitch);
#else
		SDL_LockSurface(surface);
		pixels = surface->pixels;
#endif

		ssize_t expected = bufferSize;
		ssize_t gotten;
		while ((gotten = SocketRecv(server, pixels, expected)) != expected) {
			if (gotten < 0) {
				break;
			}
			pixels = (void*) ((uintptr_t) pixels + gotten);
			expected -= gotten;
		}
		if (gotten < 0) {
			break;
		}

#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_UnlockTexture(sdlTex);
		SDL_RenderCopy(renderer, sdlTex, NULL, NULL);
		SDL_RenderPresent(renderer);
#else
		SDL_UnlockSurface(surface);
		SDL_Flip(surface);
#endif
	}
	SocketSubsystemDeinit();

	return 0;
}
