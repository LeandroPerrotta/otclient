const LoginApp = {
    state: {
        view: 'login', // 'login' | 'characters'
        email: '',
        password: '',
        remember: false,
        isLoading: false,
        error: null,
        success: null,
        characters: [],
        selectedCharacter: null,

        config: {
            apiUrl: 'http://localhost:3000/auth',
            endpoints: { login: '/login' },
            timeout: 10000
        },
        loginCredentials: null,
        remainingPremiumDays: 0,
        isGameOnline: false
    },

    actions: {
        async login() {
            LoginApp.state.isLoading = true;
            LoginApp.state.error = null;
            LoginApp.state.success = null;

            try {
                const url = LoginApp.state.config.apiUrl + LoginApp.state.config.endpoints.login;

                const response = await fetch(url, {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({
                        email: LoginApp.state.email,
                        password: LoginApp.state.password
                    })
                });

                await new Promise(resolve => setTimeout(resolve, 500));

                if (response.ok) {
                    const data = await response.json();

                    LoginApp.state.characters = data.characters || [];
                    LoginApp.state.remainingPremiumDays = data.remaining_premium_days || 0;

                    LoginApp.state.view = 'characters';

                    // Save data only if "remember me" is activated
                    if (LoginApp.state.remember) {
                        localStorage.setItem('remember_me', 'true');
                        localStorage.setItem('saved_email', LoginApp.state.email);
                        localStorage.setItem('saved_password', LoginApp.state.password);
                        localStorage.setItem('session_token', data.session_token);
                    } else {
                        // If "remember me" not checked, save only temporary token
                        localStorage.setItem('session_token', data.session_token);
                    }
                } else {
                    LoginApp.state.error = tr('Email or password incorrect');
                }
            } catch (error) {
                console.error('Login error:', error);
                LoginApp.state.error = tr('Connection error');
            } finally {
                LoginApp.state.isLoading = false;
                m.redraw();
            }
        },

        selectCharacter(character) {
            LoginApp.state.selectedCharacter = character;
            m.redraw();
        },

        async enterGame() {
            if (!LoginApp.state.selectedCharacter) {
                LoginApp.state.error = tr('Select a character to enter the game');
                m.redraw();
                return;
            }

            LoginApp.state.isLoading = true;

            try {
                const result = {
                    character: LoginApp.state.selectedCharacter,
                    credentials: {
                        email: LoginApp.state.email,
                        password: LoginApp.state.password
                    },
                    method: 'traditional'
                };

                sendToLua('login_complete', result);
                    } catch (error) {
            console.error('Error sending data to Lua:', error);
            LoginApp.state.error = tr('Error connecting to game');
        } finally {
                LoginApp.state.isLoading = false;
                m.redraw();
            }
        },

        back() {
            // Clear data and return to login screen
            LoginApp.actions.clearAllData();
            LoginApp.state.view = 'login';
            LoginApp.state.email = '';
            LoginApp.state.password = '';
            LoginApp.state.remember = false;
            LoginApp.state.selectedCharacter = null;
            LoginApp.state.error = null;
            m.redraw();
        },

        clearAllData() {
            // Clear only "Remember me" data, keep temporary token
            localStorage.removeItem('remember_me');
            localStorage.removeItem('saved_email');
            localStorage.removeItem('saved_password');
        },

        clearSessionDataOnly() {
            // Clear only session token
            localStorage.removeItem('session_token');
        },
    },

    view() {
        return m('.container.otc-window',
            LoginApp.state.view === 'login'
                ? m(LoginForm)
                : m(CharacterList)
        );
    }
};

// Login Component
const LoginForm = {
    view() {
        return m('.login-section', [
            m('.header.otc-header', [
                m('h1.otc-window-title', [
                    m('span.otc-icon', '🔐'),
                    tr('Login')
                ]),
                m('p.otc-label', tr('Enter with your credentials to access the game'))
            ]),

            // Messages
            LoginApp.state.error && m('.error-message.otc-message.otc-message-error', LoginApp.state.error),
            LoginApp.state.success && m('.success-message.otc-message.otc-message-success', LoginApp.state.success),

            // Form
            m('form', {
                onsubmit: (e) => {
                    e.preventDefault();

                    // Basic validation
                    if (!LoginApp.state.email.trim() || !LoginApp.state.password) {
                        LoginApp.state.error = tr('Please fill in all fields.');
                        m.redraw();
                        return;
                    }

                    LoginApp.actions.login();
                }
            }, [
                m('.form-group.otc-form-group', [
                    m('label.otc-menu-label[for=email]', tr('Email or Account ID')),
                    m('input.otc-text-input[type=text][id=email][placeholder="' + tr('Enter your email or account ID') + '"][required]', {
                        value: LoginApp.state.email,
                        oninput: (e) => LoginApp.state.email = e.target.value,
                        disabled: LoginApp.state.isGameOnline
                    })
                ]),

                m('.form-group.otc-form-group', [
                    m('label.otc-menu-label[for=password]', tr('Password')),
                    m('input.otc-text-input[type=password][id=password][placeholder="' + tr('Enter your password') + '"][required]', {
                        value: LoginApp.state.password,
                        oninput: (e) => LoginApp.state.password = e.target.value,
                        disabled: LoginApp.state.isGameOnline
                    })
                ]),

                m('.remember-me.otc-checkbox.otc-form-group', [
                    m('input[type=checkbox][id=remember]', {
                        checked: LoginApp.state.remember,
                        onchange: (e) => LoginApp.state.remember = e.target.checked,
                        disabled: LoginApp.state.isGameOnline
                    }),
                    m('label[for=remember]', tr('Remember me'))
                ]),

                m('button.login-button.otc-button.otc-button-primary[type=submit]', {
                    disabled: LoginApp.state.isLoading || LoginApp.state.isGameOnline
                }, [
                    LoginApp.state.isLoading && m('span#button-loading.otc-loading'),
                    m('span#button-text', LoginApp.state.isLoading ? tr('Connecting...') : (LoginApp.state.isGameOnline ? tr('Login') : tr('Enter the Game')))
                ])
            ])
        ]);
    }
};

// Character List Component
const CharacterList = {
    view() {
        return m('.character-section', [
            m('.header.otc-header', [
                m('h1.otc-window-title', [
                    m('span.otc-icon', '👥'),
                    tr('Characters')
                ]),
                m('p.otc-label', tr('Select a character to enter the game'))
            ]),

            // Account status
            m('#account-status.account-status.otc-panel-flat', [
                m('h4.otc-label', [
                    m('span.otc-icon', '📊'),
                    tr('Account Status')
                ]),
                m('p.otc-label', [
                    `${tr('Status')}: `,
                    LoginApp.state.remainingPremiumDays > 0
                        ? m('span.premium-status', `${tr('Premium')} (${LoginApp.state.remainingPremiumDays} ${tr('days')} left)`)
                        : m('span.free-status', tr('Free account'))
                ])
            ]),

            // Character list
            LoginApp.state.characters.length === 0
                ? m('.empty-state.otc-label', tr('No characters found'))
                : m('#character-list.character-list.otc-panel-flat',
                    LoginApp.state.characters.map(character =>
                        m('.character-item', {
                            class: (LoginApp.state.selectedCharacter?.name === character.name ? 'selected' : '') +
                                (LoginApp.state.isGameOnline ? ' disabled' : ''),
                            onclick: () => LoginApp.actions.selectCharacter(character)
                        }, [
                            m('.character-info', [
                                m('.character-name.otc-label', character.name),
                                m('.character-details', [
                                    m('.character-world.otc-label', character.world_name),
                                    m('span.otc-label', '•'),
                                    m('.character-level.otc-label', `${tr('Level')} ${character.level || tr('N/A')}`),
                                    m('span.otc-label', '•'),
                                    m('.character-vocation.otc-label', character.vocation || tr('N/A'))
                                ])
                            ]),
                            m('.character-outfit')
                        ])
                    )
                ),

            // Buttons
            m('.button-group.otc-button-group', [
                m('button#back-button.button-secondary.otc-button', {
                    onclick: () => LoginApp.actions.back(),
                    disabled: LoginApp.state.isGameOnline
                }, tr('Back')),
                m('button#enter-game-button.button-primary.otc-button.otc-button-primary', {
                    disabled: !LoginApp.state.selectedCharacter || LoginApp.state.isLoading || LoginApp.state.isGameOnline,
                    onclick: () => LoginApp.actions.enterGame()
                }, [
                    LoginApp.state.isLoading && m('span#enter-button-loading.otc-loading'),
                    m('span#enter-button-text', LoginApp.state.isLoading ? tr('Connecting...') : (LoginApp.state.isGameOnline ? tr('Login') : tr('Enter the Game')))
                ])
            ])
        ]);
    }
};

// Callbacks for Lua communication
window.registerLuaCallback('init_config', async (data) => {
    LoginApp.state.config = typeof data === 'string' ? JSON.parse(data) : data;

    // Check if "Remember me" is activated
    const rememberMe = localStorage.getItem('remember_me');
    const savedEmail = localStorage.getItem('saved_email');
    const savedPassword = localStorage.getItem('saved_password');

    if (rememberMe === 'true' && savedEmail && savedPassword) {
        // FLOW 2: "Remember me" activated - go directly to request
        LoginApp.state.email = savedEmail;
        LoginApp.state.password = savedPassword;
        LoginApp.state.remember = true;

        // Go directly to request (without showing login screen)
        LoginApp.actions.login();
    } else {
        // FLOW 1: First time or "Remember me" not activated - show login screen
        LoginApp.state.view = 'login';
    }

    m.redraw();
});

window.registerLuaCallback('reset_and_show_login', () => {
    // Reset state
    LoginApp.state.selectedCharacter = null;
    LoginApp.state.error = null;
    LoginApp.state.success = null;

    // Check if "Remember me" is activated
    const rememberMe = localStorage.getItem('remember_me');
    const savedEmail = localStorage.getItem('saved_email');
    const savedPassword = localStorage.getItem('saved_password');

    if (rememberMe === 'true' && savedEmail && savedPassword) {
        // FLOW 2: "Remember me" activated - go directly to request
        LoginApp.state.email = savedEmail;
        LoginApp.state.password = savedPassword;
        LoginApp.state.remember = true;

        // Go directly to request (without showing login screen)
        LoginApp.actions.login();
    } else {
        // FLOW 1: First time or "Remember me" not activated - show login screen
        LoginApp.state.view = 'login';
        LoginApp.state.email = '';
        LoginApp.state.password = '';
        LoginApp.state.remember = false;
    }

    m.redraw();
});



window.registerLuaCallback('game_state_changed', (rawData) => {
    try {
        const data = typeof rawData === 'string' ? JSON.parse(rawData) : rawData;
        const wasOnline = LoginApp.state.isGameOnline;
        LoginApp.state.isGameOnline = data.isOnline;

        // If left the game, clear messages
        if (!LoginApp.state.isGameOnline && wasOnline) {
            LoginApp.state.success = null;
            LoginApp.state.error = null;
        }

        m.redraw();
    } catch (error) {
        console.error('Error processing game state:', error);
    }
});

window.registerLuaCallback('logout', () => {
    // Logout should not clear data here
    // Cleanup will be done by reset_and_show_login based on "remember me" state
});

// Initialization
document.addEventListener('DOMContentLoaded', () => {
    // Initialize translations module first
    if (typeof WebViewTranslations !== 'undefined') {
        WebViewTranslations.init();
        WebViewTranslations.setupCallbacks();
    }

    if (typeof m === 'undefined') {
        console.error('Mithril.js was not loaded!');
        document.body.innerHTML = '<div style="padding: 20px; color: red;">Error: Mithril.js was not loaded</div>';
        return;
    }

    try {
        m.mount(document.body, LoginApp);
    } catch (error) {
        console.error('Error mounting Mithril application:', error);
        document.body.innerHTML = '<div style="padding: 20px; color: red;">Error mounting application: ' + error.message + '</div>';
        return;
    }

    // Notify that JavaScript is loaded
    sendToLua('js_loaded', '');

    // Preload module-specific translations
    WebViewTranslations.preloadModuleTranslations([
        'Characters',
        'Select a character to enter the game',
        'Account Status',
        'Status',
        'Type',
        'Level',
        'Back',
        'Enter the Game',
        'Email or Account ID',
        'Remember me',
        'Enter with your credentials to access the game',
        'Connecting...',
        'Premium Account',
        'Normal',
        'Normal Account',
        'Free account',
        'days',
        'Please fill in all fields.',
        'Email or password incorrect',
        'Connection error',
        'Error connecting to game',
        'No characters found',
        'N/A',
        'Enter your email or account ID',
        'Enter your password'
    ]);
});
