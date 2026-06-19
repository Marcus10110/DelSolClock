We need to make a number of changes to the output of the font convert script.

We should remove the forward declaration from the top of all_fonts.h

Instead, it should include correct header: #include <Adafruit_GFX.h>

I would like to generate a new enum class in all_fonts.g with the name "Font" and one entry per font. (not one per-size, just 1 per font)
I would like to auto-generate a a function with the signature: const GFXfont\* GetFont( Font font, int size )

GetFont should be implemented in a new file, all_fonts.cpp.
The body of GetFont should look like below, with 2 nested switch statements. The outer switch will be based on the font enum, and the inner switch will be based on the font size.

I would also like to rename the global "all_fonts" to "AllFonts" and leave a declaration in the header, but move the definition to the the new all_fonts.cpp file.

The goal is to allow consumers to include all_fonts.h, and then call GetFont, or iterate through the master list of fonts.

````
const GFXfont* GetFont( Font font, int size )
{
    switch( font )
    {
    case Font::AgaveNerd:
    {
        switch( size )
        {
        case 7:
            return &AgaveNerdFont_Regular7pt7b;
        case 8:
            return &AgaveNerdFont_Regular8pt7b;
        case 9:
            return &AgaveNerdFont_Regular9pt7b;
        case 10:
            return &AgaveNerdFont_Regular10pt7b;
        case 12:
            return &AgaveNerdFont_Regular12pt7b;
        case 14:
            return &AgaveNerdFont_Regular14pt7b;
        case 16:
            return &AgaveNerdFont_Regular16pt7b;
        default:
            return nullptr;
        }
    }
    break;
    case Font::AudioLink:
    {
        switch( size )
        {
        case 7:
            return &AudioLinkMono_Regular7pt7b;
        case 8:
            return &AudioLinkMono_Regular8pt7b;
        case 9:
            return &AudioLinkMono_Regular9pt7b;
        case 10:
            return &AudioLinkMono_Regular10pt7b;
        case 12:
            return &AudioLinkMono_Regular12pt7b;
        case 14:
            return &AudioLinkMono_Regular14pt7b;
        case 16:
            return &AudioLinkMono_Regular16pt7b;
        default:
            return nullptr;
        }
    }
    break;
    // ...
    ```
````
