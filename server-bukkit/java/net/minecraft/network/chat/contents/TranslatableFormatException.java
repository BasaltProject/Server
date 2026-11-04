// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents;
public class TranslatableFormatException extends IllegalArgumentException {
    public TranslatableFormatException(TranslatableContents component, String message) {
        super(message);
    }
    public TranslatableFormatException(TranslatableContents component, int index) {
        super();
    }
    public TranslatableFormatException(TranslatableContents component, Throwable t) {
        super(t);
    }
}
