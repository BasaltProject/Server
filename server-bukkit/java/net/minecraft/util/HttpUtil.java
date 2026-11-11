// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.google.common.hash.HashCode;
import com.google.common.hash.HashFunction;
import java.io.IOException;
import java.io.InputStream;
import java.io.UncheckedIOException;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.Proxy;
import java.net.ServerSocket;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;
import java.util.OptionalLong;

public class HttpUtil {
    private HttpUtil() {
    }

    public static Path downloadFile(Path targetDir, URL url, Map<String, String> headers, HashFunction hashFunction, HashCode requestedHash,
            int maxSize, Proxy proxy, DownloadProgressListener listener) {
        HttpURLConnection connection = null;
        boolean success = false;
        try {
            listener.requestStart();
            Path target = targetDir.resolve(requestedHash.toString());
            if (Files.exists(target) && hashFunction.hashBytes(Files.readAllBytes(target)).equals(requestedHash)) {
                success = true;
                return target;
            }
            connection = (HttpURLConnection) url.openConnection(proxy);
            headers.forEach(connection::setRequestProperty);
            long length = connection.getContentLengthLong();
            listener.downloadStart(length >= 0 ? OptionalLong.of(length) : OptionalLong.empty());
            if (length > maxSize) {
                throw new IOException("Filesize is bigger than maximum allowed (file is " + length + ", limit is " + maxSize + ")");
            }
            Files.createDirectories(targetDir);
            Path temporary = Files.createTempFile(targetDir, "download", ".tmp");
            long total = 0L;
            try (InputStream in = connection.getInputStream()) {
                byte[] chunk = new byte[8192];
                java.io.OutputStream out = Files.newOutputStream(temporary);
                try (out) {
                    int read;
                    while ((read = in.read(chunk)) != -1) {
                        total += read;
                        if (total > maxSize) {
                            throw new IOException("Filesize was bigger than maximum allowed (got >= " + total + ", limit was " + maxSize + ")");
                        }
                        out.write(chunk, 0, read);
                        listener.downloadedBytes(total);
                    }
                }
            }
            if (!hashFunction.hashBytes(Files.readAllBytes(temporary)).equals(requestedHash)) {
                Files.deleteIfExists(temporary);
                throw new IOException("Hash of downloaded file did not match the requested hash");
            }
            Files.move(temporary, target, java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            success = true;
            return target;
        } catch (IOException e) {
            throw new UncheckedIOException("Failed to download file " + url, e);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
            listener.requestFinished(success);
        }
    }

    public static int getAvailablePort() {
        try (ServerSocket socket = new ServerSocket(0)) {
            return socket.getLocalPort();
        } catch (IOException e) {
            return 25564;
        }
    }

    public static boolean isPortAvailable(int port) {
        if (port < 0 || port > 65535) {
            return false;
        }
        try (ServerSocket socket = new ServerSocket(port, 0, InetAddress.getLoopbackAddress())) {
            return true;
        } catch (IOException e) {
            return false;
        }
    }

    public interface DownloadProgressListener {
        void requestStart();

        void downloadStart(OptionalLong sizeBytes);

        void downloadedBytes(long bytesSoFar);

        void requestFinished(boolean success);
    }
}
