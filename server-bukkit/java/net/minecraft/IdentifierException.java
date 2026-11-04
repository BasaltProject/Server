// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;
public class IdentifierException extends RuntimeException {
    public IdentifierException(String message) {
        super(message);
    }
    public IdentifierException(String message, Throwable cause) {
        super(message, cause);
    }
}
