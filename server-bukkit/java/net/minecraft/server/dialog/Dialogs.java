// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.network.chat.CommonComponents;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.DialogTags;
public class Dialogs {
    public static final ResourceKey<Dialog> SERVER_LINKS = named("server_links");
    public static final ResourceKey<Dialog> CUSTOM_OPTIONS = named("custom_options");
    public static final ResourceKey<Dialog> QUICK_ACTIONS = named("quick_actions");
    public static final int BIG_BUTTON_WIDTH = 310;
    private static final ActionButton kBack = new ActionButton(new CommonButtonData(CommonComponents.GUI_BACK, 200), Optional.empty());
    private static ResourceKey<Dialog> named(String id) {
        return ResourceKey.create(Registries.DIALOG, Identifier.withDefaultNamespace(id));
    }
    private static CommonDialogData menu(String key) {
        return new CommonDialogData(Component.translatable("menu." + key + ".title"), Optional.of(Component.translatable("menu." + key)),
                true, true, DialogAction.CLOSE, List.of(), List.of());
    }
    public static void bootstrap(BootstrapContext<Dialog> context) {
        HolderGetter<Dialog> dialogs = context.lookup(Registries.DIALOG);
        context.register(SERVER_LINKS, new ServerLinksDialog(menu("server_links"), Optional.of(kBack), 1, BIG_BUTTON_WIDTH));
        context.register(CUSTOM_OPTIONS, new DialogListDialog(menu("custom_options"), dialogs.getOrThrow(DialogTags.PAUSE_SCREEN_ADDITIONS),
                Optional.of(kBack), 1, BIG_BUTTON_WIDTH));
        context.register(QUICK_ACTIONS, new DialogListDialog(menu("quick_actions"), dialogs.getOrThrow(DialogTags.QUICK_ACTIONS),
                Optional.of(kBack), 1, BIG_BUTTON_WIDTH));
    }
}
