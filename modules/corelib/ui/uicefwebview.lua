function UICEFWebView:loadComponent(component_name, component_path)

    local css_tag_template = '<link rel="stylesheet" type="text/css" href="{{ css_path }}">'
    local js_tag_template = '<script type="text/javascript" src="{{ js_path }}"></script>'

    local resolvePathFn = g_resources.resolvePath

    local css_file_list = { resolvePathFn('webviews/default.css'), resolvePathFn(component_path .. '/' .. component_name .. '.css')}
    local js_file_list = { resolvePathFn('webviews/default.js'), resolvePathFn(component_path .. '/' .. component_name .. '.js')}

    local absolutePathFn = g_resources.getRealPath

    local css_styles = ''
    local js_scripts = ''

    for _, css_file in ipairs(css_file_list) do
        local css_path = 'file://' .. absolutePathFn(css_file)
        local css_tag = css_tag_template:gsub('{{ css_path }}', css_path)
        css_styles = css_styles .. css_tag
    end
    
    for _, js_file in ipairs(js_file_list) do
        local js_path = 'file://' .. absolutePathFn(js_file)
        local js_tag = js_tag_template:gsub('{{ js_path }}', js_path)
        js_scripts = js_scripts .. js_tag
    end

    local html = g_resources.readFileContents('webviews/index.template.html')
    local body_content = g_resources.readFileContents(resolvePathFn(component_path .. '/' .. component_name .. '.html'))

    html = html:gsub('{{ css_styles }}', css_styles)
    html = html:gsub('{{ js_scripts }}', js_scripts)
    html = html:gsub('{{ body_content }}', body_content)


    print(html) 
    self:loadHtml(html)
end