local M = {}

M.languages = {}

function M.add_language(lang, font_hash, text)
	M.languages[lang] = {font_hash = font_hash, text = text}
	--print("languages: ", "add_language", lang)
end

function M.remove_language(lang)
	--print("languages: ", "remove_language", lang)
	M.languages[lang] = nil
end

function M.get_language(lang)
	--print("languages: ", "get_language", lang)
	return M.languages[lang]
end

return M