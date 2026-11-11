// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.parsing.packrat;

public record Atom<T>(String name) {
    public static <T> Atom<T> of(String name) {
        return new Atom<>(name);
    }

    @Override
    public String toString() {
        return "<" + name + ">";
    }
}
