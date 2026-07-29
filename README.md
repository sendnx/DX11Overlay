# DX11 Overlay Inspector — Bilingual Case Study

[![Windows CI](https://github.com/sendnx/DX11Overlay/actions/workflows/windows-ci.yml/badge.svg)](https://github.com/sendnx/DX11Overlay/actions/workflows/windows-ci.yml)

> Türkçe ve English. Bu depo bir “nasıl kurulur?” belgesi değil; modern C++ ile
> bir DirectX 11 overlay sisteminin nasıl tasarlanacağını inceleyen, adım adım
> okunabilir bir mühendislik çalışmasıdır.

Bu proje yalnızca içinde çalıştığı prosesin belleğini **okur**. Bellek yazma,
koruma atlatma, anti-cheat müdahalesi veya üçüncü taraf proses kontrolü içermez.
Örnekleri yalnızca sahibi olduğunuz ya da test izniniz bulunan yazılımlarda
kullanın.

Tamamlayıcı belgeler: [derleme ve doğrulama](BUILDING.md),
[kod bilmeyenler için başlangıç rehberi](readme-1.md),
[gerçek doğrulama sonuçları](VALIDATION.md),
[katkı rehberi](CONTRIBUTING.md), [güvenlik politikası](SECURITY.md),
[değişiklik günlüğü](CHANGELOG.md), [MIT lisansı](LICENSE) ve
[üçüncü taraf bildirimleri](THIRD_PARTY_NOTICES.md).

Companion documents: [build and validation](BUILDING.md),
[beginner-friendly Turkish guide](readme-1.md),
[recorded validation results](VALIDATION.md),
[contributing](CONTRIBUTING.md), [security policy](SECURITY.md),
[changelog](CHANGELOG.md), [MIT license](LICENSE), and
[third-party notices](THIRD_PARTY_NOTICES.md).

---

## Türkçe ders

### 1. Problem nedir?

Bir DX11 uygulamasının çizim döngüsüne küçük bir araç arayüzü eklemek istiyoruz.
Arayüz:

- doğru zamanda çizilmeli,
- pencere mesajlarını almalı,
- swap-chain yeniden boyutlandığında bozulmamalı,
- prosesin okunabilir bellek bölgelerini güvenle inceleyebilmeli,
- kapanırken hook, thread, ImGui ve COM kaynaklarını doğru sırada bırakmalıdır.

İlk bakışta bu yalnızca “`Present` fonksiyonuna hook at ve ImGui çiz” problemi
gibi görünür. Asıl çalışma ise **yaşam döngüsü yönetimidir**. Renderer henüz
oluşmamış olabilir, bir resize sırasında back buffer geçersizleşebilir veya
kapanış başladığında başka bir thread hook callback'inin içinde olabilir.

### 2. Sistemi parçalara ayırma

Projede her sınıfın tek bir sorumluluğu vardır:

| Bileşen | Sorumluluk | İncelenecek dosya |
|---|---|---|
| `Application` | Hook ve renderer yaşam döngüsü, ImGui ekranları | [`Application.cpp`](src/Application.cpp) |
| `Memory` | Adres doğrulama ve kontrollü okuma | [`Memory.cpp`](src/Memory.cpp) |
| `PatternScanner` | İmza ayrıştırma, PE section tarama | [`PatternScanner.cpp`](src/PatternScanner.cpp) |
| `Logger` | Thread-safe, sınırlı log geçmişi | [`Logger.cpp`](src/Logger.cpp) |
| `DllMain` | Minimum entry point ve açık `Start/Stop/Wait` API'si | [`DllMain.cpp`](src/DllMain.cpp) |
| Tests | Saf mantık, yaşam döngüsü ve gerçek DX11 smoke kontrolleri | [`tests/`](tests) |

Bu ayrım önemli: pattern parser'ı test etmek için DX11 renderer başlatmak
zorunda kalmıyoruz; bellek okuyucu da ImGui hakkında hiçbir şey bilmiyor.

### 3. Case study: `Present` adresini nasıl buluyoruz?

`IDXGISwapChain` bir COM interface'idir. Metot adresleri nesnenin vtable'ında
bulunur. Ancak gerçek uygulamanın swap-chain pointer'ı başlangıçta elimizde
yoktur. Çözüm olarak geçici bir pencere ve geçici DX11 swap-chain oluşturulur:

1. Görünmeyen/geçici bir Win32 pencere sınıfı oluştur.
2. Bu pencereye bağlı geçici bir DX11 swap-chain üret.
3. Swap-chain vtable'ından `Present` ve `ResizeBuffers` adreslerini al.
4. Geçici COM nesnelerini ve pencereyi bırak.
5. Aynı DXGI implementasyonundaki fonksiyon adreslerine MinHook kur.

Buradaki kritik fikir şudur: geçici swap-chain'i render etmek için değil,
**interface implementasyonunun adreslerini keşfetmek için** kullanıyoruz.

Projede kullanılan vtable indeksleri:

```cpp
void** vtable = *reinterpret_cast<void***>(swapChain.Get());
presentTarget = vtable[8];
resizeTarget  = vtable[13];
```

Bu yaklaşımın sınırı, hedef uygulamanın gerçekten DX11/DXGI kullanmasıdır.
Birden fazla swap-chain varsa geçerli ve pozitif istemci alanına sahip ilk
pencereye ait olan seçilir.

### 4. Renderer neden ilk `Present` sırasında oluşturuluyor?

Dummy swap-chain yalnızca adres keşfi içindir. Gerçek `ID3D11Device`,
`ID3D11DeviceContext`, output window ve back buffer hedef uygulamanın
swap-chain'inden alınmalıdır. Bu nedenle ImGui kurulumu ilk gerçek `Present`
callback'ine kadar ertelenir.

Temel akış:

```text
LoadLibrary tamamlandı → host StartOverlay çağırır
   ↓
Dummy swap-chain → vtable adresleri → hook kurulumu
   ↓
İlk gerçek Present
   ↓
Device + context + window + render target
   ↓
ImGui frame → overlay çizimi → original Present
```

Bu, “lazy initialization” örneğidir: gerekli gerçek kaynak ulaşılabilir olduğu
anda oluşturulur.

### 5. Resize problemi

`ResizeBuffers` çağrıldığında eski back buffer artık kullanılamaz. Eski
`ID3D11RenderTargetView` tutulmaya devam edilirse çizim hatası veya crash
oluşabilir.

Doğru sıra:

1. Overlay render mutex'ini al.
2. Eski render target'ı bırak.
3. Orijinal `ResizeBuffers` fonksiyonunu çağır.
4. Çağrı başarılıysa yeni back buffer'dan render target oluştur.

Bu yüzden yalnızca `Present` hook'u yeterli değildir; kaynak yaşam döngüsünün
bir parçası olan resize olayı da ele alınır.

### 6. Güvenli bellek okuma modeli

Bir adresin sıfırdan farklı olması onun okunabilir olduğu anlamına gelmez.
[`Memory.cpp`](src/Memory.cpp) önce `VirtualQuery` ile adresin bulunduğu bölgeyi
inceler:

- bölge `MEM_COMMIT` olmalı,
- `PAGE_NOACCESS` veya `PAGE_GUARD` olmamalı,
- istenen **tüm aralık** okunabilir bölgelerde kalmalı,
- `address + size` hesabı taşmamalıdır.

Sonra okuma, MSVC üzerinde küçük ve C++ nesnesi sahiplenmeyen bir SEH
fonksiyonunda yapılır. SEH ile `std::string` gibi destructor gerektiren
nesneleri aynı fonksiyonda kullanmamak önemlidir; aksi halde MSVC `C2712`
hatası üretilebilir.

Katmanlar şu şekildedir:

```text
Read<T>
  └─ CopyReadable
       ├─ IsRangeReadable
       │    └─ VirtualQuery
       └─ CopyWithSeh
```

Bu model bir “adres kesinlikle sonsuza kadar güvenlidir” garantisi vermez.
Başka bir thread bölgeyi sorgudan hemen sonra serbest bırakabilir. SEH kopyalama
adımı bu yarışın proses çökmesine dönüşmesini engelleyen ikinci savunmadır.

### 7. Pointer chain semantiği

Pointer chain fonksiyonlarının en sık problemi, offset'in dereference'tan önce
mi sonra mı uygulandığının belirsiz olmasıdır. Bu projede kural açıktır:

```text
current = base
current += offset[0] → pointer oku
current += offset[1] → pointer oku
...
current += son offset → final adresi döndür
```

Örneğin:

```cpp
struct Leaf { int value; };
struct Root { Leaf* leaf; };

ResolvePointerChain(
    reinterpret_cast<uintptr_t>(&root),
    {offsetof(Root, leaf), offsetof(Leaf, value)});
```

Sonuç `leaf.value` alanının adresidir; değer otomatik olarak okunmaz. Bu ayrım
API'nin davranışını tahmin edilebilir yapar.

### 8. Pattern scanner nasıl çalışıyor?

İmza:

```text
48 89 5C 24 ? 57 48 81 EC ?? ?? ?? ??
```

önce şu ara gösterime çevrilir:

```text
[0x48, 0x89, 0x5C, 0x24, wildcard, 0x57, ...]
```

Parser yalnızca iki haneli hex byte, `?` ve `??` kabul eder. Hatalı token bütün
pattern'ı geçersiz yapar.

Scanner bütün adres uzayını körlemesine dolaşmaz:

1. Seçilen modülün DOS ve NT header'larını doğrular.
2. PE section tablosunu okur.
3. Discardable section'ları atlar.
4. Her section içindeki okunabilir memory region'ları belirler.
5. Bölgeyi güvenli bir snapshot'a kopyalar.
6. Pattern eşleşmesini snapshot üzerinde yapar.
7. En fazla 64 sonuç döndürür ve stop token'ı düzenli olarak kontrol eder.

Snapshot üzerinde arama yapmanın iki faydası vardır: inner loop içinde sürekli
`VirtualQuery` çağrılmaz ve arama sırasında ham proses adresi tekrar tekrar
dereference edilmez.

### 9. UI neden worker thread'den çizilmiyor?

Dear ImGui frame state'i tek bir render thread'inde tutulmalıdır. Worker thread
yalnızca pattern sonuçlarını üretir. Sonuçları mutex korumalı bir veri yapısına
yazar; `Present` thread'i bir sonraki frame'de snapshot'ı ekrana basar.

Paylaşılan durumlar üç gruba ayrılır:

- Basit bayraklar: `std::atomic<bool>`
- Birlikte tutarlı kalması gereken listeler/metinler: `std::mutex`
- DX11/ImGui kaynakları: render mutex'i ve render thread'i

Bu ayrım, her değişkene rastgele mutex eklemekten daha anlaşılırdır.

### 10. Kontrollü kapanış neden ayrı bir özellik?

Bir DLL'in hook'u aktifken veya worker thread'i hâlâ kendi kodunu yürütürken
unload edilmesi, instruction pointer'ın serbest bırakılmış kod sayfalarında
kalmasına neden olabilir.

Projede kapanış sırası:

1. Yeni işi durdur ve `stop_token` gönder.
2. Worker thread'leri join et.
3. Hook girişlerini disable et.
4. Devam eden render/resize kritik bölümünün bitmesini bekle.
5. Orijinal WndProc'u geri yükle.
6. ImGui backend'lerini ve context'i kapat.
7. DX11 `ComPtr` kaynaklarını bırak.
8. Hook'ları remove et; gerekiyorsa MinHook'u uninitialize et.
9. Global application pointer'ını en son temizle.

Başlangıç `DllMain` içinde yapılmaz. Loader lock serbest kaldıktan sonra host
`StartOverlay()` çağırır. Kapanış protokolü:

```cpp
RequestOverlayShutdown();
if (WaitForOverlayShutdown(INFINITE) == WAIT_OBJECT_0) {
    FreeLibrary(overlayModule);
}
```

`WaitForOverlayShutdown` retained OS thread handle'ını bekler; yalnızca
`Application` pointer'ının temizlenmesini beklemez. Böylece fonksiyon
`WAIT_OBJECT_0` döndürdüğünde overlay thread'i gerçekten sona ermiştir.
`IsOverlayRunning()` da aynı thread handle'ının durumunu sorgular ve bilinmeyen
bir hata durumunda güvenli tarafta kalarak `TRUE` döndürür.

### 11. RAII burada ne kazandırıyor?

- `ComPtr`: COM referans sayısını scope sonunda otomatik azaltır.
- `std::jthread`: thread yaşamını ve stop token'ını birlikte yönetir.
- `std::vector` / `std::string`: buffer sahipliğini açık hale getirir.
- `Application` destructor'ı: normal çıkış ve hata çıkışını aynı cleanup
  yolunda birleştirir.

RAII bütün problemleri otomatik çözmez. Hook disable sırası veya callback
yarışları hâlâ açıkça tasarlanmalıdır. RAII, doğru tasarlanmış sıranın yarıda
kalmasını önler.

### 12. Bu case study nasıl çalışılmalı?

Önerilen okuma sırası:

1. [`Memory.hpp`](include/overlay/Memory.hpp) ile public API'yi incele.
2. [`Memory.cpp`](src/Memory.cpp) içinde doğrulama katmanlarını takip et.
3. [`PatternScanner.cpp`](src/PatternScanner.cpp) içinde PE section keşfini ve
   snapshot taramasını oku.
4. [`Application::InstallHooks`](src/Application.cpp) ile adres keşfini izle.
5. `PresentHook`, `ResizeBuffersHook` ve `Shutdown` fonksiyonlarını birlikte
   okuyarak kaynak yaşam döngüsünü çıkar.
6. [`OverlayTests.cpp`](tests/OverlayTests.cpp) içindeki testlerin hangi
   sözleşmeleri sabitlediğini incele.

Kendine şu soruları sor:

- Renderer init yarıda başarısız olursa hangi kaynaklar bırakılıyor?
- Resize ile Present aynı anda gelirse hangi mutex sınırı koruma sağlıyor?
- Pattern taraması sırasında modül unload edilirse hangi savunmalar var?
- Shutdown bayrağı callback'in hangi noktalarında kontrol ediliyor?
- Yeni bir feature eklediğinde sahibi hangi thread olacak?

### 13. Öğrenci alıştırmaları

Kolaydan zora:

1. Section tablosuna `VirtualSize` ve entropy sütunu ekle.
2. Hex viewer'a ASCII arama ve satır adresini kopyalama ekle.
3. Pattern sonucunu `module + relative offset` olarak sakla.
4. Scanner'ı sabit boyutlu chunk'larla çalıştırarak büyük section
   allocation'ını azalt.
5. Seçilecek swap-chain için pencere başlığı/PID/ölçü politikası tasarla.
6. Renderer init hata yolları için bir state machine çiz.
7. DirectX kaynaklarını mock edilebilir interface'lerin arkasına alarak hook
   dışındaki uygulama mantığını unit test edilebilir hale getir.

### 14. Tasarımın bilinçli sınırları

- Araç yalnızca aynı prosesin belleğini okur.
- Birden fazla swap-chain için gelişmiş seçim politikası yoktur.
- Device removal/recreation, resize kadar ayrıntılı ele alınmamıştır.
- Hook tekniği DX11/DXGI ABI varsayımlarına bağlıdır.
- Pattern eşleşmeleri semantik doğrulama yapmaz; bulunan byte dizisinin gerçekten
  beklenen fonksiyon olduğunu çağıranın doğrulaması gerekir.

Bu sınırlar eksiklik listesinden çok sonraki tasarım iterasyonu için başlangıç
noktalarıdır.

Proje [MIT Lisansı](LICENSE) altında dağıtılır.

---

## English lesson

### 1. What problem are we solving?

We want to add a small inspection UI to the rendering loop of a DX11
application. The UI must:

- render at the correct point in the frame,
- receive window messages,
- survive swap-chain resizing,
- inspect readable regions of its own process safely,
- release hooks, threads, ImGui, and COM resources in the correct order.

This initially looks like “hook `Present` and draw ImGui.” The real engineering
problem is **lifecycle management**. The renderer may not exist yet, a resize
invalidates the back buffer, and another thread may still be executing a hook
callback when shutdown begins.

### 2. Decomposing the system

Each component has one primary responsibility:

| Component | Responsibility | Source |
|---|---|---|
| `Application` | Hook and renderer lifecycle, ImGui views | [`Application.cpp`](src/Application.cpp) |
| `Memory` | Address validation and guarded reads | [`Memory.cpp`](src/Memory.cpp) |
| `PatternScanner` | Signature parsing and PE section scanning | [`PatternScanner.cpp`](src/PatternScanner.cpp) |
| `Logger` | Thread-safe bounded log history | [`Logger.cpp`](src/Logger.cpp) |
| `DllMain` | Minimal entry point and explicit `Start/Stop/Wait` API | [`DllMain.cpp`](src/DllMain.cpp) |
| Tests | Pure logic, lifecycle, and real DX11 smoke checks | [`tests/`](tests) |

This separation lets us test the pattern parser without starting DX11, while
the memory reader remains completely independent of ImGui.

### 3. Case study: discovering `Present`

`IDXGISwapChain` is a COM interface whose method addresses live in a vtable.
At startup, however, we do not own the real application's swap-chain pointer.
The project creates a temporary Win32 window and DX11 swap chain:

1. Register a temporary window class.
2. Create a temporary DX11 swap chain for that window.
3. Read the `Present` and `ResizeBuffers` entries from its vtable.
4. Release every temporary COM object and destroy the window.
5. Install MinHook on the discovered DXGI implementation addresses.

The temporary swap chain is not used for rendering. It is an **address
discovery mechanism**.

### 4. Why initialize during the first real `Present`?

The device, immediate context, output window, and back buffer must come from the
target application's real swap chain. ImGui initialization is therefore
deferred until the first suitable `Present` callback.

```text
LoadLibrary returns → host calls StartOverlay
   ↓
Temporary swap chain → vtable addresses → hooks
   ↓
First real Present
   ↓
Device + context + window + render target
   ↓
ImGui frame → overlay rendering → original Present
```

This is lazy initialization: resources are created only when their real owner
becomes available.

### 5. The resize case

`ResizeBuffers` invalidates the previous back buffer. Keeping the old
`ID3D11RenderTargetView` can cause rendering errors or a crash.

The correct sequence is:

1. Lock the overlay render state.
2. Release the old render target.
3. Call the original `ResizeBuffers`.
4. Recreate the target from the new back buffer if resizing succeeded.

This is why a production-minded overlay needs more than a `Present` hook.

### 6. The safe-read model

A non-null address is not necessarily readable. [`Memory.cpp`](src/Memory.cpp)
uses `VirtualQuery` to require that:

- every region is committed,
- no region is guarded or `PAGE_NOACCESS`,
- the complete requested range is readable,
- address arithmetic does not overflow.

The copy then runs inside a small MSVC SEH helper that owns no C++ objects
requiring stack unwinding. Keeping `std::string` and similar objects outside the
SEH function avoids MSVC error `C2712`.

```text
Read<T>
  └─ CopyReadable
       ├─ IsRangeReadable
       │    └─ VirtualQuery
       └─ CopyWithSeh
```

Validation alone cannot eliminate a query/use race: another thread could free
the region immediately after `VirtualQuery`. The guarded copy is the second
line of defense.

### 7. Pointer-chain semantics

Pointer-chain helpers are often ambiguous about whether an offset is applied
before or after dereferencing. This project defines one explicit contract:

```text
current = base
current += offset[0] → read pointer
current += offset[1] → read pointer
...
current += final offset → return address
```

The final address is returned without reading its value. The test suite fixes
this contract so that a future refactor cannot silently change it.

### 8. How the pattern scanner works

A signature such as:

```text
48 89 5C 24 ? 57 48 81 EC ?? ?? ?? ??
```

is parsed into bytes and wildcard entries. Invalid tokens reject the whole
pattern.

The scanner does not walk the entire address space blindly:

1. Validate the selected module's DOS and NT headers.
2. Read its PE section table.
3. Skip discardable sections.
4. Locate readable memory regions inside each section.
5. Copy each region into a guarded snapshot.
6. Match the pattern against that snapshot.
7. Stop at 64 results and observe the stop token periodically.

Snapshot scanning reduces repeated system calls in the inner loop and avoids
repeatedly dereferencing raw process addresses.

### 9. Concurrency model

Dear ImGui frame state stays on the render thread. The scanner worker only
produces results and publishes them through mutex-protected state. The next
`Present` frame renders that state.

Shared data is divided by behavior:

- independent flags use `std::atomic<bool>`,
- lists and related text use `std::mutex`,
- DX11 and ImGui objects are owned by the render path and render mutex.

Ownership by thread is more useful than adding locks without a model.

### 10. Shutdown as a first-class feature

Unloading a DLL while a hook or worker is executing its code can leave an
instruction pointer inside released pages.

The project shuts down in this order:

1. Reject new work and request worker cancellation.
2. Join worker threads.
3. Disable hook entry points.
4. Wait for active render/resize critical sections.
5. Restore the original window procedure.
6. Shut down ImGui backends and context.
7. Release DX11 COM resources.
8. Remove hooks and uninitialize owned MinHook state.
9. Clear the global application pointer last.

Initialization is not performed inside `DllMain`. The host calls
`StartOverlay()` after the loader lock has been released. The shutdown
protocol is:

```cpp
RequestOverlayShutdown();
if (WaitForOverlayShutdown(INFINITE) == WAIT_OBJECT_0) {
    FreeLibrary(overlayModule);
}
```

`WaitForOverlayShutdown` waits on a retained OS thread handle rather than an
application pointer. `WAIT_OBJECT_0` therefore means that the overlay thread
has actually exited. `IsOverlayRunning()` queries the same handle and
conservatively reports `TRUE` if the OS thread state cannot be determined.

### 11. What RAII contributes

- `ComPtr` automatically releases COM references.
- `std::jthread` ties thread lifetime to cancellation and scope.
- Standard containers make buffer ownership explicit.
- The `Application` destructor unifies normal and error cleanup.

RAII does not decide the shutdown order for us. It ensures that a correctly
designed order is consistently executed.

### 12. How to study this case

Recommended reading order:

1. Read the public contract in [`Memory.hpp`](include/overlay/Memory.hpp).
2. Follow the validation layers in [`Memory.cpp`](src/Memory.cpp).
3. Study PE discovery and snapshot matching in
   [`PatternScanner.cpp`](src/PatternScanner.cpp).
4. Follow address discovery in `Application::InstallHooks`.
5. Read `PresentHook`, `ResizeBuffersHook`, and `Shutdown` together as one
   resource-lifecycle story.
6. Inspect [`OverlayTests.cpp`](tests/OverlayTests.cpp) to see which contracts
   are intentionally fixed.

Questions to ask while reading:

- Which resources are released if renderer initialization fails halfway?
- What protects concurrent resize and present operations?
- What defenses remain if a module changes while it is being scanned?
- At which callback boundaries is shutdown observed?
- Which thread should own any new feature you add?

### 13. Exercises

From introductory to advanced:

1. Add `VirtualSize` and entropy columns to the section table.
2. Add ASCII search and “copy row address” to the hex viewer.
3. Store scanner results as `module + relative offset`.
4. Scan large sections in fixed-size overlapping chunks.
5. Design a swap-chain selection policy using window title and dimensions.
6. Model renderer initialization and failure paths as a state machine.
7. Hide DirectX resources behind testable interfaces and unit-test the
   non-hook application logic.

### 14. Intentional limitations

- The inspector reads only its own process.
- Multi-swap-chain selection is deliberately simple.
- Device removal/recreation is not handled as deeply as resizing.
- Hook discovery depends on DX11/DXGI ABI assumptions.
- A byte-pattern match does not prove semantic identity; callers must validate
  what a result represents.

These limitations are useful starting points for the next design iteration,
not merely a list of missing features.

The project is distributed under the [MIT License](LICENSE).
