// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.repository;

import java.util.function.UnaryOperator;
import net.minecraft.network.chat.Component;

public interface PackSource {
    UnaryOperator<Component> NO_DECORATION = component -> component;
    PackSource DEFAULT = create(NO_DECORATION, true);
    PackSource BUILT_IN = create(decorateWithSource("pack.source.builtin"), true);
    PackSource FEATURE = create(decorateWithSource("pack.source.feature"), false);
    PackSource WORLD = create(decorateWithSource("pack.source.world"), true);
    PackSource SERVER = create(decorateWithSource("pack.source.server"), true);

    Component decorate(Component packDescription);

    boolean shouldAddAutomatically();

    static PackSource create(UnaryOperator<Component> decorator, boolean addAutomatically) {
        return new PackSource() {
            public Component decorate(Component packDescription) {
                return decorator.apply(packDescription);
            }

            public boolean shouldAddAutomatically() {
                return addAutomatically;
            }
        };
    }

    private static UnaryOperator<Component> decorateWithSource(String key) {
        Component source = Component.translatable(key);
        return description -> Component.translatable("pack.nameAndSource", description, source);
    }
}
