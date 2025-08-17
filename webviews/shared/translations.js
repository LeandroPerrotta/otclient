// Internal module state
const WebViewTranslations = {
    state: {
        translations: {}, // Translation cache
        pendingTranslations: {}, // Pending translations to avoid duplicate requests
        localeInfo: null // Locale information
    },

    // Initialize module
    init() {
        console.log('WebViewTranslations initialized');
        this.preloadCommonTranslations();
    },

    // Function to request translation from Lua
    requestTranslation(key) {
        if (typeof sendToLua === 'function') {
            sendToLua('translate', key);
        } else {
            console.warn('sendToLua is not available');
        }
    },

    // Function to request multiple translations from Lua
    requestTranslations(keys) {
        if (typeof sendToLua === 'function') {
            sendToLua('translate_multiple', keys.join(','));
        } else {
            console.warn('sendToLua is not available');
        }
    },

    // Preload common translations
    preloadCommonTranslations() {
        const commonKeys = [
            'Back',
            'Loading...',
            'Error',
            'Success',
            'Cancel',
            'Confirm',
            'Yes',
            'No',
            'OK',
            'Close',
            'Save',
            'Delete',
            'Edit',
            'Add',
            'Remove',
            'Search',
            'Filter',
            'Sort',
            'Refresh',
            'Settings',
            'Options',
            'Help',
            'About'
        ];

        this.requestTranslations(commonKeys);
    },

    // Preload module-specific translations
    preloadModuleTranslations(keys) {
        this.requestTranslations(keys);
    },

    // Configure Lua callbacks
    setupCallbacks() {
        if (typeof window !== 'undefined' && window.registerLuaCallback) {
            // Callback to receive locale information
            window.registerLuaCallback('set_locale_info', (rawData) => {
                try {
                    const data = typeof rawData === 'string' ? JSON.parse(rawData) : rawData;
                    this.state.localeInfo = data;
                } catch (error) {
                    console.error('Error loading locale information:', error);
                }
            });

            // Callback to receive single translation result
            window.registerLuaCallback('translation_result', (key, translation) => {
                try {
                    this.state.translations[key] = translation;
                    // Clear from pending list
                    if (this.state.pendingTranslations) {
                        delete this.state.pendingTranslations[key];
                    }
                } catch (error) {
                    console.error('Error processing translation:', error);
                }
            });

            // Callback to receive multiple translations result
            window.registerLuaCallback('translations_result', (rawData) => {
                try {
                    const translations = typeof rawData === 'string' ? JSON.parse(rawData) : rawData;

                    // Update cache
                    Object.assign(this.state.translations, translations);

                    // Clear all pending
                    this.state.pendingTranslations = {};

                    // Force redraw if Mithril is available
                    if (typeof m !== 'undefined' && typeof m.redraw === 'function') {
                        setTimeout(() => m.redraw(), 0);
                    }
                } catch (error) {
                    console.error('Error processing translations:', error);
                }
            });
        }
    },



    // Get current locale information
    getLocaleInfo() {
        return this.state.localeInfo;
    },

    // Clear translation cache
    clearCache() {
        this.state.translations = {};
        this.state.pendingTranslations = {};
    },

    // Check if a translation is available
    hasTranslation(key) {
        return this.state.translations.hasOwnProperty(key);
    },

    // Get all cached translations
    getAllTranslations() {
        return { ...this.state.translations };
    }
};

// Global translation function
function tr(key, ...args) {
    // If we already have the translation in cache, use it
    if (WebViewTranslations.state.translations[key]) {
        const translation = WebViewTranslations.state.translations[key];
        if (args.length > 0) {
            return translation.replace(/%s/g, () => args.shift());
        }
        return translation;
    }

    // If we already requested this translation, don't request again
    if (WebViewTranslations.state.pendingTranslations && WebViewTranslations.state.pendingTranslations[key]) {
        return key;
    }

    // Mark as pending and request from Lua
    if (!WebViewTranslations.state.pendingTranslations) {
        WebViewTranslations.state.pendingTranslations = {};
    }
    WebViewTranslations.state.pendingTranslations[key] = true;
    WebViewTranslations.requestTranslation(key);

    // Return original key while we don't have the translation
    return key;
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = WebViewTranslations;
}
