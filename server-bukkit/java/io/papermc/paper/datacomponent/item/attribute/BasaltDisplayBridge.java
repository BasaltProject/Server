package io.papermc.paper.datacomponent.item.attribute;

import java.util.LinkedHashMap;
import net.kyori.adventure.text.ComponentLike;
import org.basaltmc.bukkit.datacomponent.ItemSpecs;

public final class BasaltDisplayBridge implements AttributeModifierDisplayBridge {
    @Override
    public AttributeModifierDisplay.Default reset() {
        return (AttributeModifierDisplay.Default) ItemSpecs.DISPLAY_DEFAULT.value(new LinkedHashMap<>());
    }

    @Override
    public AttributeModifierDisplay.Hidden hidden() {
        return (AttributeModifierDisplay.Hidden) ItemSpecs.DISPLAY_HIDDEN.value(new LinkedHashMap<>());
    }

    @Override
    public AttributeModifierDisplay.OverrideText override(ComponentLike text) {
        return ItemSpecs.override(text);
    }
}
