// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.dialog.Dialog;

public final class DialogTags {
    public static final TagKey<Dialog> PAUSE_SCREEN_ADDITIONS = org.basaltmc.bukkit.facade.Tags.key(Registries.DIALOG, "pause_screen_additions");
    public static final TagKey<Dialog> QUICK_ACTIONS = org.basaltmc.bukkit.facade.Tags.key(Registries.DIALOG, "quick_actions");

    private DialogTags() {}
}
