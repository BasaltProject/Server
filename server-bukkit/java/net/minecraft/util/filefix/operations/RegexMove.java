// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.operations;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.stream.Stream;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import net.minecraft.util.filefix.FileFixUtil;
import net.minecraft.util.worldupdate.UpgradeProgress;

public record RegexMove(Pattern fromPattern, String toReplacement) implements FileFixOperation {
    public RegexMove(String fromPattern, String toPattern) {
        this(Pattern.compile(fromPattern), toPattern);
    }

    @Override
    public void fix(Path baseDirectory, UpgradeProgress progress) throws IOException {
        if (!Files.exists(baseDirectory) || !Files.isDirectory(baseDirectory)) {
            return;
        }
        try (Stream<Path> files = Files.list(baseDirectory)) {
            for (Path file : files.toList()) {
                String name = file.getFileName().toString();
                Matcher matcher = fromPattern.matcher(name);
                if (matcher.matches()) {
                    FileFixUtil.moveFile(baseDirectory, name, matcher.replaceAll(toReplacement));
                }
            }
        }
    }
}
