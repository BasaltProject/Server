// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.virtualfilesystem;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.nio.file.Path;
import net.minecraft.util.ExtraCodecs;

public record FileMove(Path from, Path to) {
    public static Codec<FileMove> moveCodec(Path fromDirectory, Path toDirectory) {
        return RecordCodecBuilder.create(instance -> instance.group(
            ExtraCodecs.guardedPathCodec(fromDirectory).fieldOf("from").forGetter(FileMove::from),
            ExtraCodecs.guardedPathCodec(toDirectory).fieldOf("to").forGetter(FileMove::to)
        ).apply(instance, FileMove::new));
    }
}
