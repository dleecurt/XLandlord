-- docs/rules-pdf.lua - pandoc filter for the PDF of RULES.md (make pdf).
--
-- RULES.md stays plain Markdown that reads well on its own (and on GitHub);
-- what only the printed page needs is done here:
--   * the hand-written Contents section is dropped (the PDF has its own),
--   * the Quick reference starts on a page of its own, to be printed alone,
--   * each picture gets a size that suits it on the page,
--   * pictures stay where they are in the text instead of floating away.

local sizes = {
  ["board-map.png"]    = "100%",
  ["game-board.png"]   = "100%",
  ["joiner-view.png"]  = "90%",
  ["new-game.png"]     = "85%",
  ["for-sale.png"]     = "85%",
  ["which-way.png"]    = "85%",
  ["the-deal.png"]     = "55%",
  ["waiting-room.png"] = "70%",
}

-- Size a picture by its file name.
function Image(img)
  local name = img.src:match("([^/]+)$")
  if sizes[name] then img.attributes.width = sizes[name] end
  return img
end

-- Drop "Contents" and the list under it; start the quick reference on a
-- new page.
function Pandoc(doc)
  local out, skip = {}, false
  for _, b in ipairs(doc.blocks) do
    if b.t == "Header" then
      local text = pandoc.utils.stringify(b)
      skip = (text == "Contents")
      if text:match("Quick reference") then
        table.insert(out, pandoc.RawBlock("latex", "\\clearpage"))
      end
    end
    if not skip then table.insert(out, b) end
  end
  doc.blocks = out
  return doc
end
