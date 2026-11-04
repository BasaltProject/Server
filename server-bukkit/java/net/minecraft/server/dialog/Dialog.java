// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.dialog.action.Action;
public interface Dialog {
    Codec<Integer> WIDTH_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.server.dialog.Dialog.class, "WIDTH_CODEC");
    Codec<Dialog> DIRECT_CODEC = org.basaltmc.bukkit.facade.DialogCodecs.DIRECT_CODEC;
    Codec<Holder<Dialog>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.server.dialog.Dialog.class, "CODEC");
    Codec<HolderSet<Dialog>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.server.dialog.Dialog.class, "LIST_CODEC");
    StreamCodec<RegistryFriendlyByteBuf, Holder<Dialog>> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.holder(net.minecraft.core.registries.Registries.DIALOG, net.minecraft.network.codec.ByteBufCodecs.fromCodecWithRegistriesTrusted(DIRECT_CODEC));
    StreamCodec<ByteBuf, Dialog> CONTEXT_FREE_STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.fromCodecTrusted(DIRECT_CODEC);
    CommonDialogData common();
    MapCodec<? extends Dialog> codec();
    Optional<Action> onCancel();
}
