/**
 * Toast Notification System
 * Lightweight, mobile-friendly notifications
 */

class ToastManager {
    constructor() {
        this.container = this.createContainer();
        this.toasts = [];
        this.maxToasts = 5;
    }
    
    createContainer() {
        // Check if container already exists
        let container = document.getElementById('toast-container');
        if (!container) {
            container = document.createElement('div');
            container.id = 'toast-container';
            container.className = 'toast-container';
            document.body.appendChild(container);
        }
        return container;
    }
    
    show(message, type = 'info', duration = 3000) {
        // Limit number of toasts
        if (this.toasts.length >= this.maxToasts) {
            this.remove(this.toasts[0]);
        }
        
        const toast = new Toast(message, type, duration);
        this.toasts.push(toast);
        this.container.appendChild(toast.element);
        toast.show();
        
        // Auto-remove after duration
        setTimeout(() => {
            this.remove(toast);
        }, duration + 300); // Add transition time
        
        return toast;
    }
    
    remove(toast) {
        const index = this.toasts.indexOf(toast);
        if (index > -1) {
            this.toasts.splice(index, 1);
            toast.hide();
        }
    }
    
    success(message, duration = 3000) {
        return this.show(message, 'success', duration);
    }
    
    error(message, duration = 4000) {
        return this.show(message, 'error', duration);
    }
    
    info(message, duration = 3000) {
        return this.show(message, 'info', duration);
    }
    
    warning(message, duration = 3500) {
        return this.show(message, 'warning', duration);
    }
    
    clear() {
        this.toasts.forEach(toast => toast.hide());
        this.toasts = [];
    }
}

class Toast {
    constructor(message, type, duration) {
        this.message = message;
        this.type = type;
        this.duration = duration;
        this.element = this.createElement();
    }
    
    createElement() {
        const toast = document.createElement('div');
        toast.className = `toast toast-${this.type}`;
        
        const icon = this.getIcon();
        const closeBtn = document.createElement('button');
        closeBtn.className = 'toast-close';
        closeBtn.innerHTML = '&times;';
        closeBtn.setAttribute('aria-label', 'Close notification');
        
        const iconSpan = document.createElement('span');
        iconSpan.className = 'toast-icon';
        iconSpan.textContent = icon;
        
        const messageSpan = document.createElement('span');
        messageSpan.className = 'toast-message';
        messageSpan.textContent = this.message;
        
        toast.appendChild(iconSpan);
        toast.appendChild(messageSpan);
        toast.appendChild(closeBtn);
        
        // Close button handler
        closeBtn.addEventListener('click', () => {
            this.hide();
        });
        
        return toast;
    }
    
    getIcon() {
        const icons = {
            success: '✓',
            error: '✕',
            warning: '⚠',
            info: 'ℹ'
        };
        return icons[this.type] || icons.info;
    }
    
    show() {
        // Trigger reflow to enable transition
        this.element.offsetHeight;
        requestAnimationFrame(() => {
            this.element.classList.add('toast-show');
        });
    }
    
    hide() {
        this.element.classList.remove('toast-show');
        setTimeout(() => {
            if (this.element.parentNode) {
                this.element.parentNode.removeChild(this.element);
            }
        }, 300);
    }
}

// Global instance - will be initialized when DOM is ready
window.toast = null;

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.toast = new ToastManager();
    });
} else {
    window.toast = new ToastManager();
}

// Global helper function for backward compatibility
function showToast(message, type = 'info', duration = 3000) {
    if (window.toast) {
        return window.toast.show(message, type, duration);
    }
    console.warn('Toast system not initialized');
}
