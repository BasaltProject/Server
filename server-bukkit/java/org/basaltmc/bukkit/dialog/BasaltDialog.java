package org.basaltmc.bukkit.dialog;

import io.papermc.paper.dialog.Dialog;
import io.papermc.paper.registry.data.dialog.DialogBase;
import io.papermc.paper.registry.data.dialog.type.DialogType;
import java.util.Map;
import org.bukkit.NamespacedKey;

public final class BasaltDialog implements Dialog {
    private final DialogBase base;
    private final DialogType type;

    public BasaltDialog(DialogBase base, DialogType type) {
        this.base = java.util.Objects.requireNonNull(base, "base");
        this.type = java.util.Objects.requireNonNull(type, "type");
    }

    public DialogBase base() {
        return base;
    }

    public DialogType type() {
        return type;
    }

    Map<String, Object> toData() {
        return DialogNbt.dialog(base, type);
    }

    public String snbt() {
        return DialogNbt.snbt(toData());
    }

    @Override
    public NamespacedKey getKey() {
        throw new IllegalStateException("A dialog made by a plugin is not in the dialog registry");
    }
}
