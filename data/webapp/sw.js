/**
 * Service Worker - Offline Support for PWA
 */

const CACHE_NAME = 'lora-sniffer-v1.0.0';
const STATIC_ASSETS = [
    '/',
    '/index.html',
    '/css/style.css',
    '/js/app.js',
    '/js/api-client.js',
    '/js/map.js',
    '/manifest.json',
    // External resources cached for offline use
    'https://unpkg.com/leaflet@1.9.4/dist/leaflet.css',
    'https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'
];

/**
 * Install event - cache static assets
 */
self.addEventListener('install', (event) => {
    console.log('[ServiceWorker] Installing...');
    
    event.waitUntil(
        caches.open(CACHE_NAME).then(cache => {
            console.log('[ServiceWorker] Caching static assets');
            return cache.addAll(STATIC_ASSETS);
        }).catch(err => {
            console.error('[ServiceWorker] Cache failed:', err);
        })
    );
    
    // Activate immediately
    self.skipWaiting();
});

/**
 * Activate event - clean up old caches
 */
self.addEventListener('activate', (event) => {
    console.log('[ServiceWorker] Activating...');
    
    event.waitUntil(
        caches.keys().then(cacheNames => {
            return Promise.all(
                cacheNames.map(cacheName => {
                    if (cacheName !== CACHE_NAME) {
                        console.log('[ServiceWorker] Deleting old cache:', cacheName);
                        return caches.delete(cacheName);
                    }
                })
            );
        })
    );
    
    // Take control immediately
    return self.clients.claim();
});

/**
 * Fetch event - serve from cache, fallback to network
 */
self.addEventListener('fetch', (event) => {
    const { request } = event;
    
    // Skip non-GET requests
    if (request.method !== 'GET') {
        return;
    }
    
    // Skip API requests (always fetch fresh)
    if (request.url.includes('/api/')) {
        return;
    }
    
    // Skip WebSocket requests
    if (request.url.includes('/ws')) {
        return;
    }
    
    event.respondWith(
        caches.match(request).then(response => {
            if (response) {
                // Serve from cache
                return response;
            }
            
            // Fetch from network
            return fetch(request).then(networkResponse => {
                // Cache successful responses
                if (networkResponse && networkResponse.status === 200) {
                    const responseClone = networkResponse.clone();
                    caches.open(CACHE_NAME).then(cache => {
                        cache.put(request, responseClone);
                    });
                }
                
                return networkResponse;
            });
        }).catch(() => {
            // Offline fallback
            if (request.destination === 'document') {
                return caches.match('/index.html');
            }
        })
    );
});

/**
 * Message event - handle messages from app
 */
self.addEventListener('message', (event) => {
    if (event.data && event.data.type === 'SKIP_WAITING') {
        self.skipWaiting();
    }
});
