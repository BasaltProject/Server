// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.selector.options;

import java.util.HashSet;
import java.util.Set;
import net.minecraft.resources.Identifier;

public class InvertableSetOptionState {
    private Limitation limitation = Limitation.NONE;
    private final Set<Identifier> tags = new HashSet<>();

    public boolean canParsePositiveElement() {
        return limitation == Limitation.NONE;
    }

    public boolean canParseNegativeElement() {
        return limitation != Limitation.SINGLE;
    }

    public boolean canParseAnyTag() {
        return limitation != Limitation.SINGLE;
    }

    public boolean canParseTag(Identifier tag) {
        return canParseAnyTag() && !tags.contains(tag);
    }

    public boolean canParseAny() {
        return limitation != Limitation.SINGLE;
    }

    public boolean canParseElement(boolean inverted) {
        return inverted ? canParseNegativeElement() : canParsePositiveElement();
    }

    public void markParsedPositiveElement() {
        limitation = Limitation.SINGLE;
    }

    public void markParsedNegativeElement() {
        limitation = Limitation.MULTIPLE;
    }

    public void markParsedTag(Identifier tag) {
        tags.add(tag);
        limitation = Limitation.MULTIPLE;
    }

    public void markParsedElement(boolean inverted) {
        if (inverted) {
            markParsedNegativeElement();
        } else {
            markParsedPositiveElement();
        }
    }

    static enum Limitation {
        NONE, SINGLE, MULTIPLE;
    }
}
