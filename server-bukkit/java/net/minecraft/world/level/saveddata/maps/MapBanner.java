// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.saveddata.maps;
import net.minecraft.world.level.block.entity.BannerBlockEntity;
import net.minecraft.world.level.block.AbstractBannerBlock;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.chat.Component;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.level.BlockGetter;
public record MapBanner(BlockPos pos, DyeColor color, Optional<Component> name) {
    public static final Codec<MapBanner> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.saveddata.maps.MapBanner.class, "CODEC");
    public static MapBanner fromWorld(BlockGetter level, BlockPos pos) {
        if (level.getBlockEntity(pos) instanceof BannerBlockEntity banner) {
            return new MapBanner(pos, (banner.getBlockState().getBlock() instanceof AbstractBannerBlock block ? block.getColor() : DyeColor.WHITE), Optional.ofNullable(banner.getCustomName()));
        }
        return null;
    }
    public Holder<MapDecorationType> getDecoration() {
        return switch (color) {
            case WHITE -> MapDecorationTypes.WHITE_BANNER;
            case ORANGE -> MapDecorationTypes.ORANGE_BANNER;
            case MAGENTA -> MapDecorationTypes.MAGENTA_BANNER;
            case LIGHT_BLUE -> MapDecorationTypes.LIGHT_BLUE_BANNER;
            case YELLOW -> MapDecorationTypes.YELLOW_BANNER;
            case LIME -> MapDecorationTypes.LIME_BANNER;
            case PINK -> MapDecorationTypes.PINK_BANNER;
            case GRAY -> MapDecorationTypes.GRAY_BANNER;
            case LIGHT_GRAY -> MapDecorationTypes.LIGHT_GRAY_BANNER;
            case CYAN -> MapDecorationTypes.CYAN_BANNER;
            case PURPLE -> MapDecorationTypes.PURPLE_BANNER;
            case BLUE -> MapDecorationTypes.BLUE_BANNER;
            case BROWN -> MapDecorationTypes.BROWN_BANNER;
            case GREEN -> MapDecorationTypes.GREEN_BANNER;
            case RED -> MapDecorationTypes.RED_BANNER;
            case BLACK -> MapDecorationTypes.BLACK_BANNER;
        };
    }
    public String getId() {
        return "banner-" + pos.getX() + "," + pos.getY() + "," + pos.getZ();
    }
}
