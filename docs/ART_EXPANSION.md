# Artwork for the new relics

The `assets/art/relics-expansion.tga` atlas contains 16 original illustrated medallions for relics with IDs 24–39. It was created with the built-in `image_gen` tool on October 30, 2026, from a written art brief: embossed metal, ivory, enamel, and a casino-jester ornament style. Common uses soft ivory, Rare uses blue enamel, and Legendary uses gold. The original atlas of 24 illustrations was not changed.

The tool's PNG was converted to an uncompressed 32-bit TGA without changing the RGBA pixels; the image is 1254×1254. Because the source layout does not form a perfectly uniform grid, the interface uses explicit UV rectangles based on the medallions' actual bounds. This prevents adjacent artwork from appearing in an icon.

Row order:

1. Rewind, Redraw, Forked Path, Lifeline.
2. Velvet Dividend, Lucky Seven, Twin Sigil, Mixed Company.
3. Crescendo, Ace High, Silver Lining, Loaded Dice.
4. Alchemist, Phoenix, Royal Decree, Grand Finale.

The source PNG and full prompt are preserved among the review materials in `palace-refinement/relics-expansion.png` and `palace-refinement/atlas-prompt.txt` in the task working directory. These materials are not required to run the game. Rendering uses only the TGA; no new game dependencies were added.
