# DX11 Overlay Inspector — Hiç Kod Bilmeyenler İçin Çok Basit Anlatım

Bu belge, “C++ nedir?”, “DirectX ne işe yarar?” veya “DLL ne demek?” diye
soran birinin bile projeyi anlayabilmesi için yazıldı.

Burada hızlı gitmeyeceğiz. Önce kelimeleri öğreneceğiz, sonra parçaları tek tek
tanıyacağız, en sonunda bütün sistemin nasıl birlikte çalıştığını göreceğiz.

> Kısa cevap: Bu proje, bir DirectX 11 uygulamasının çizdiği görüntünün üzerine
> küçük bir bilgi ve inceleme paneli çizer. Yalnızca içinde çalıştığı programın
> okunmasına izin verilen bellek alanlarını inceler.

## 1. Önce güvenlik ve izin

Bu proje bir eğitim çalışmasıdır.

- Başka insanların programlarını izinsiz incelemek için kullanılmamalıdır.
- Belleğe veri yazmaz.
- Güvenlik sistemlerini aşmaya çalışmaz.
- Anti-cheat sistemlerine müdahale etmez.
- Başka bir prosesi uzaktan kontrol etmez.
- Yalnızca kendi prosesinin belleğini okur.

“Proses”, o anda çalışan bir program demektir. Örneğin hesap makinesini
açtığında Windows onun için bir proses oluşturur.

Bu projeyi yalnızca:

- kendi yazdığın uygulamalarda,
- sahibinin açıkça izin verdiği test uygulamalarında,
- öğrenme amacıyla hazırladığın deneme ortamlarında

kullanmalısın.

## 2. Bu proje ne yapıyor?

Bir sinema perdesi düşün.

Film sürekli yeni görüntüler gösteriyor. Biz filmin kendisini değiştirmeden,
perdenin önüne şeffaf bir asetat koyuyoruz. Asetatın üzerinde küçük bir menü,
bilgiler ve yeşil bir çember var.

İşte **overlay**, bu şeffaf asetat gibidir.

Bu projedeki overlay şunları gösterebilir:

- çalışan programın yüklediği modüller,
- bu modüllerin bellekteki başlangıç adresleri,
- PE section adı verilen bölümler,
- okunabilen bir adresin içindeki baytlar,
- belirli bir bayt dizisinin nerelerde bulunduğu,
- program içinde oluşan bilgi ve hata mesajları.

Menü `INSERT` tuşuyla açılıp kapanır. `END` tuşu temiz bir kapanış ister.

## 3. Bilmemiz gereken kelimeler

Bu bölüm küçük bir sözlüktür. Bir kelimeyi unutursan buraya geri dönebilirsin.

### Program

Bilgisayara ne yapacağını söyleyen komutların bütünüdür.

### Kaynak kod

İnsanların okuyabileceği program metnidir. Bu projede kaynak kodun büyük kısmı
C++ diliyle yazılmıştır.

### C++

Bilgisayara hızlı ve ayrıntılı işler yaptırmak için kullanılan bir programlama
dilidir.

### Derlemek

C++ ile yazılmış insan tarafından okunabilir kodu, bilgisayarın
çalıştırabileceği makine koduna çevirmektir.

Bunu bir çevirmen gibi düşünebilirsin:

```text
C++ kaynak kodu → derleyici → bilgisayarın anlayacağı dosya
```

### DLL

DLL, başka bir programın gerektiğinde yükleyebildiği kod paketi gibidir.
Bu projenin derlenmiş ana çıktısı `DX11Overlay.dll` adını alır.

Bir alet çantası düşün. Ana program çantayı açar ve içindeki araçları kullanır.
DLL de buna benzer.

### DirectX 11 veya DX11

Windows uygulamalarının ekran kartıyla konuşmasına yardım eden bir teknoloji
ailesidir. DX11, ekrana üç boyutlu veya iki boyutlu görüntü çizmek için
kullanılır.

### Frame

Ekranda gösterilen tek bir resimdir.

Hareketli görüntü aslında art arda gösterilen çok sayıda resimden oluşur:

```text
1. resim → 2. resim → 3. resim → 4. resim → ...
```

Bir uygulama saniyede 60 frame çiziyorsa, gözümüz bunları akıcı bir hareket
gibi görür.

### Renderer

Ekrana neyin, nasıl çizileceğini yöneten bölümdür. Türkçede “çizici” gibi
düşünebilirsin.

### ImGui

Buton, sekme, yazı ve tablo gibi arayüz parçalarını kolayca çizmemizi sağlayan
bir kütüphanedir.

Bu projedeki menüyü ImGui oluşturur.

### Hook

Hook kelimesi “kanca” anlamına gelir. Bir fonksiyon çağrılırken araya kontrollü
bir durak eklemeye benzer.

Bir kapı düşün:

```text
Normal durum:
Ziyaretçi → kapı → oda

Hook bulunan durum:
Ziyaretçi → görevli → kapı → oda
```

Görevli ziyaretçiyi tamamen durdurmaz. Önce küçük bir iş yapar, sonra normal
yola devam etmesine izin verir.

Bu projede görevli, uygulamanın frame sunma işlemi sırasında overlay'i çizer.
Ardından orijinal fonksiyon çağrılır ve uygulama normal çalışmasına devam eder.

### Bellek

Çalışan programların geçici bilgilerini tuttuğu alandır. Büyük ve çok uzun bir
dolap gibi düşünebilirsin.

Dolabın her çekmecesinin bir numarası vardır. Bellekte bu numaraya **adres**
denir.

### Bayt

Bellekte saklanan küçük bir veri parçasıdır. Baytlar çoğu zaman onaltılık
biçimde gösterilir:

```text
00 4A FF 10
```

### Modül

Çalışan bir programın yüklediği ana `.exe` dosyası veya DLL gibi kod
parçalarından biridir.

### Thread

Bir programın aynı anda ilerleyen iş yollarından biridir.

Bir restoran düşün:

- bir çalışan sipariş alıyor,
- biri yemek hazırlıyor,
- biri masaları temizliyor.

Hepsi aynı restoranda ama farklı işleri yürütüyor. Thread'ler de buna benzer.

### Mutex

Aynı anda yalnızca bir thread'in belirli bir alana girmesini sağlayan kilittir.

Tek anahtarlı bir oda düşün. Anahtar kimdeyse o içeri girer. Diğerleri anahtar
geri gelene kadar bekler.

### COM ve `ComPtr`

DirectX nesnelerinin yaşamını yönetmek için kullanılan Windows sistemlerinden
biri COM'dur.

`ComPtr`, işi biten DirectX nesnelerini bırakmayı kolaylaştıran akıllı bir
tutucudur. Oyuncağını kullandıktan sonra otomatik olarak kutusuna koyan yardımcı
gibi düşünebilirsin.

### RAII

RAII, bir kaynağı alan nesnenin işi bitince o kaynağı geri bırakması fikridir.

Örneğin:

```text
Odaya gir → ışığı aç → işini yap → odadan çıkarken ışık otomatik kapansın
```

Bu yaklaşım, hata oluştuğunda kaynakların unutulmasını azaltır.

## 4. Ekrana bir görüntü nasıl geliyor?

Bir DX11 uygulamasında süreç çok basitleştirilmiş hâliyle şöyledir:

1. Uygulama yeni frame için nesneleri hazırlar.
2. DirectX bu nesneleri çizer.
3. Hazır görüntü arka tamponda bekler.
4. `Present` fonksiyonu görüntüyü kullanıcıya sunar.

Arka tamponu, sahne arkasında hazırlanan yeni bir resim gibi düşünebilirsin.
`Present` çağrısı da “Hazır olan resmi şimdi göster!” komutudur.

```text
Yeni resmi hazırla
       ↓
Arka tampon
       ↓
Present
       ↓
Monitörde görünen frame
```

Overlay'in doğru zamanda çizilmesi gerekir. Çok erken çizersek uygulama daha
sonra üstünü kapatabilir. Çok geç kalırsak o frame çoktan ekrana gitmiş olur.

Bu yüzden proje `Present` çağrısını takip eder.

## 5. Swap chain nedir?

Tek bir resmi çizerken kullanıcı yarım kalmış görüntüyü görmemelidir. Bu nedenle
genellikle bir görüntü gösterilirken diğeri arka tarafta hazırlanır.

Bunu iki resim kartıyla çalışan bir sihirbaz gibi düşün:

- öndeki kart seyirciye gösterilir,
- arkadaki kart hazırlanır,
- hazır olunca kartlar değiştirilir.

Bu kart değiştirme düzenini yöneten yapıya **swap chain** denir.

`IDXGISwapChain`, DX11 dünyasında bu sistemi temsil eden arayüzdür.

## 6. Proje gerçek `Present` adresini nasıl buluyor?

Başlangıçta projenin elinde gerçek uygulamanın swap chain'i yoktur. Fakat hook
kurabilmek için `Present` fonksiyonunun adresini bilmesi gerekir.

Proje bunun için küçük bir deneme masası kurar:

1. Geçici bir Windows penceresi oluşturur.
2. Bu pencere için geçici bir DX11 swap chain oluşturur.
3. Swap chain'in fonksiyon listesini bulur.
4. Listeden `Present` ve `ResizeBuffers` adreslerini alır.
5. Geçici pencereyi ve geçici DirectX nesnelerini kapatır.
6. Bulunan adreslere MinHook ile hook kurar.

Bu geçici pencere kullanıcıya bir şey göstermek için yapılmaz. Yalnızca doğru
adresleri öğrenmek için kullanılan bir prova sahnesidir.

Fonksiyon listesine **vtable** denir. Bir kumandanın tuş listesine
benzetebiliriz:

```text
vtable[0]  → birinci görev
vtable[1]  → ikinci görev
...
vtable[8]  → Present
...
vtable[13] → ResizeBuffers
```

Projede ilgili bölüm
[`Application::InstallHooks`](src/Application.cpp) fonksiyonundadır.

## 7. MinHook ne yapıyor?

MinHook, Windows fonksiyon çağrılarına güvenli biçimde küçük yönlendirmeler
eklemeye yardım eden bir kütüphanedir.

Proje iki önemli fonksiyonu takip eder:

- `Present`: Her yeni görüntü sunulurken overlay'i çizebilmek için.
- `ResizeBuffers`: Pencere veya görüntü boyutu değiştiğinde çizim kaynağını
  yenilemek için.

Hook'un amacı orijinal işi yok etmek değildir. Genel akış şöyledir:

```text
Present çağrıldı
       ↓
Bizim PresentHook çalıştı
       ↓
Overlay çizildi
       ↓
Orijinal Present çalıştı
       ↓
Frame ekrana gitti
```

## 8. Neden renderer hemen başlatılmıyor?

Geçici swap chain yalnızca adres bulmaya yarar. Overlay'i gerçek ekrana çizmek
için gerçek uygulamanın:

- DX11 cihazı,
- device context'i,
- penceresi,
- back buffer'ı

gereklidir.

Bu bilgiler ilk uygun gerçek `Present` çağrısında erişilebilir olur. Proje de
renderer'ı o ana kadar bekletir.

Buna **lazy initialization**, yani “gerektiği anda başlatma” denir.

Bir kafede garson gelmeden sipariş fişi hazırlamaya çalışmadığımızı düşün.
Gerekli kişi ve bilgiler gelince işlem başlar.

Tam başlangıç hikâyesi:

```text
Ana program DLL'i yükler
          ↓
StartOverlay çağrılır
          ↓
Geçici pencere ve swap chain oluşturulur
          ↓
Present ve ResizeBuffers adresleri bulunur
          ↓
Hook'lar kurulur
          ↓
İlk gerçek Present beklenir
          ↓
Gerçek DX11 kaynakları alınır
          ↓
ImGui başlatılır
          ↓
Overlay çizilir
```

## 9. Pencere boyutu değişince ne olur?

Bir resmi küçük bir çerçeve için hazırladığını düşün. Sonra biri çerçeveyi
büyüttü. Eski boyuttaki resim artık yeni çerçeveye uygun değildir.

DX11 uygulamasında pencere boyutu değişince `ResizeBuffers` çağrılabilir.
Eski back buffer artık geçerli olmayabilir. Overlay eski çizim hedefini
kullanmaya devam ederse:

- görüntü bozulabilir,
- çizim başarısız olabilir,
- program çökebilir.

Bu yüzden proje şu sırayı izler:

1. Render kilidini alır.
2. Eski render target'ı bırakır.
3. Orijinal `ResizeBuffers` fonksiyonunu çağırır.
4. İşlem başarılıysa yeni back buffer'ı alır.
5. Yeni render target oluşturur.
6. Kilidi bırakır.

Bu işlemi [`ResizeBuffersHook`](src/Application.cpp) yönetir.

## 10. Overlay menüsünde neler var?

Menü birkaç sekmeye ayrılmıştır.

### Overview

Genel durum ekranıdır.

- Renderer hazır mı?
- PE header kontrolü sağlıklı mı?
- Kaç modül yüklü?
- Yaklaşık FPS değeri nedir?
- ImGui demo penceresi gösterilsin mi?
- İmlecin çevresindeki çember çizilsin mi?

### Modules

Programın yüklediği modülleri gösterir.

Her modül için:

- adı,
- bellekteki başlangıç adresi,
- yaklaşık boyutu

görülebilir.

Bir modül seçildiğinde PE section'ları da listelenir.

### Scanner

Bir bayt deseninin seçili modülde nerelerde bulunduğunu arar.

Örnek desen:

```text
48 89 5C 24 ? 57
```

Buradaki `?`, “bu noktadaki değer ne olursa olsun kabul et” demektir.

### Hex Viewer

Bir bellek adresindeki baytları onaltılık biçimde gösterir.

Örneğin:

```text
48 65 6C 6C 6F
```

Bu değerler ASCII olarak `Hello` metnine karşılık gelebilir.

### Log

Programın kendi bilgi, uyarı ve hata mesajlarını gösterir.

Bir uçağın kara kutusu kadar ayrıntılı değildir; fakat “hangi işlem başarılı
oldu?” veya “nerede hata çıktı?” sorularına yardım eder.

## 11. Bellek neden dikkatli okunmalı?

Belleği uzun bir apartmana benzetelim.

- Her daire bir bellek bölgesi olsun.
- Her dairenin bir adresi olsun.
- Bazı dairelere girmeye izin verilsin.
- Bazıları boş, kilitli veya korumalı olsun.

Elimizde bir adres olması, orayı okuyabileceğimiz anlamına gelmez.

Yanlış bir adresi doğrudan okumaya çalışmak programı çökertebilir. Bu nedenle
[`Memory.cpp`](src/Memory.cpp) önce Windows'a sorar:

- Bu bölge gerçekten ayrılmış mı?
- Okumaya izin var mı?
- Koruma alarmı var mı?
- İstenen verinin tamamı güvenli sınırlar içinde mi?
- Adres hesabında taşma var mı?

Bu sorgu `VirtualQuery` ile yapılır.

Basitleştirilmiş akış:

```text
Bir adres geldi
      ↓
Adres sıfır mı?
      ↓ hayır
Bölge gerçekten var mı?
      ↓ evet
Okuma izni var mı?
      ↓ evet
İstenen aralığın tamamı uygun mu?
      ↓ evet
Veriyi kontrollü biçimde kopyala
```

### Neden iki ayrı güvenlik kontrolü var?

Bir oda kontrol edildiğinde açık olabilir. Fakat içeri girene kadar başka biri
kapıyı kilitleyebilir.

Bellekte de benzer bir yarış olabilir:

1. Proje bölgenin okunabilir olduğunu görür.
2. Başka bir thread bölgeyi serbest bırakır.
3. Proje okumaya çalışır.

Bu nedenle MSVC derlemesinde kopyalama işlemi ayrıca SEH koruması altında
yapılır. İlk kontrol riski azaltır, ikinci koruma beklenmedik değişimin programı
çökertmesini önlemeye çalışır.

Bu yine de “her adres sonsuza kadar güvenlidir” anlamına gelmez. Bellek canlı ve
değişken bir ortamdır.

## 12. Pointer chain nedir?

Bir hazine avı düşün:

1. İlk kâğıtta ikinci kâğıdın yeri yazıyor.
2. İkinci kâğıtta üçüncü kâğıdın yeri yazıyor.
3. Son kâğıt hazine sandığını gösteriyor.

Bellekte bir adresin başka bir adresi göstermesine pointer denir. Birden çok
pointer'ı sırayla takip etmeye pointer chain denir.

Bu projedeki kural şöyledir:

```text
Başlangıç adresini al
      ↓
İlk offset'i ekle ve oradaki pointer'ı oku
      ↓
İkinci offset'i ekle ve oradaki pointer'ı oku
      ↓
Son offset'i ekle
      ↓
Son adresi döndür
```

**Offset**, başlangıç noktasından kaç adım ilerleyeceğimizi söyleyen sayıdır.

Fonksiyon son adresteki değeri otomatik okumaz. Yalnızca son adresi verir.
Bu ayrım önemlidir: “evin adresini öğrenmek” ile “evin içindeki eşyayı almak”
aynı şey değildir.

## 13. Pattern scanner nedir?

Büyük bir kitapta belirli bir harf dizisini aradığını düşün.

Bellek tarayıcısı da seçilen modülün uygun bölümlerinde belirli bir bayt
dizisini arar.

Örnek:

```text
48 89 5C 24 ? 57 48
```

Anlamı:

- ilk bayt `48` olmalı,
- ikinci bayt `89` olmalı,
- üçüncü bayt `5C` olmalı,
- dördüncü bayt `24` olmalı,
- beşinci bayt herhangi bir değer olabilir,
- altıncı bayt `57` olmalı,
- yedinci bayt `48` olmalı.

`?` ve `??` joker değerlerdir. Bir kart oyunundaki joker gibi, o konumda her
değerle eşleşebilir.

### Scanner bütün belleği rastgele mi geziyor?

Hayır. Daha düzenli davranır:

1. Seçilen modülün geçerli bir Windows modülü olduğunu kontrol eder.
2. Modülün PE section tablosunu okur.
3. Kullanılmaması gereken bölümleri atlar.
4. Okunabilir bellek parçalarını bulur.
5. Parçayı güvenli bir kopyaya, yani snapshot'a alır.
6. Deseni bu kopyanın içinde arar.
7. En fazla 64 sonuç toplar.
8. Kapatma isteği geldiyse aramayı durdurur.

Snapshot, o an çekilmiş bir fotoğraf gibidir. Sürekli hareket eden gerçek
belleğe tekrar tekrar bakmak yerine fotoğrafın üzerinde arama yapmak daha
düzenlidir.

## 14. PE section ne demek?

Windows program dosyaları farklı amaçlara sahip bölümlere ayrılabilir.

Bir okul çantası düşün:

- bir gözde kitaplar,
- bir gözde kalemler,
- küçük gözde anahtarlar.

PE dosyasındaki section'lar da kodu ve verileri görevlerine göre ayırır.
Sık görülen örnekler:

- `.text`: çoğunlukla çalıştırılabilir kod,
- `.data`: değiştirilebilir veriler,
- `.rdata`: salt okunur veriler.

Her programda bütün adlar aynı olmak zorunda değildir. Proje section adının
yanında adresini, boyutunu ve erişim özelliklerini de gösterir.

## 15. Neden birden fazla thread var?

Ekran çizimi hızlı kalmalıdır. Pattern taraması uzun sürerse bunu çizim
thread'inde yapmak görüntünün takılmasına neden olabilir.

Bu yüzden görevler ayrılır:

- Render thread'i overlay'i çizer.
- Scanner worker thread'i bayt desenini arar.
- Inspector worker belirli sağlık kontrollerini yapar.

Ancak farklı çalışanlar aynı deftere aynı anda yazarsa karışıklık çıkabilir.
Bu nedenle ortak bilgiler atomik bayraklarla veya mutex ile korunur.

```text
Scanner worker
   │
   │ sonuçları güvenli alana yazar
   ▼
Mutex ile korunan sonuç listesi
   ▲
   │ bir sonraki frame'de okur
   │
Render thread
```

ImGui çizimi worker thread'de yapılmaz. ImGui frame bilgileri render tarafında
tutulur.

## 16. `atomic`, `mutex` ve render kilidi neden farklı?

Her veri aynı biçimde korunmaz.

### `std::atomic<bool>`

Yalnızca “evet/hayır” gibi küçük ve bağımsız bayraklar için uygundur.

Örnek:

- program çalışıyor mu?
- kapanış başladı mı?
- tarama devam ediyor mu?

### `std::mutex`

Birbiriyle birlikte doğru kalması gereken daha büyük bilgiler için kullanılır.

Örnek:

- tarama sonuçları,
- tarama durum yazısı,
- log listesi.

### Render mutex

DX11 ve ImGui kaynaklarının `Present`, `ResizeBuffers` ve kapanış sırasında
aynı anda değiştirilmesini engeller.

Her kapıya aynı kilidi takmak yerine, hangi odanın hangi kilide ihtiyacı
olduğunu düşünmek daha doğrudur.

## 17. Kapanış neden bu kadar önemli?

Bir işçi hâlâ bir binanın içindeyken binayı yıkamazsın.

Benzer şekilde:

- hook callback'i hâlâ çalışırken,
- worker thread kod yürütürken,
- ImGui kaynakları kullanılırken

DLL bellekten kaldırılırsa program çökebilir.

Bu yüzden “pencereyi kapat ve her şeyi bir anda sil” yapılmaz.

Projede kapanış sırası şöyledir:

1. Yeni iş başlatılmasını durdur.
2. Worker thread'lere durma isteği gönder.
3. Worker thread'lerin bitmesini bekle.
4. Yeni hook girişlerini devre dışı bırak.
5. Devam eden render veya resize işinin tamamlanmasını bekle.
6. Pencerenin eski mesaj yöneticisini geri koy.
7. ImGui'nin DX11 ve Win32 parçalarını kapat.
8. ImGui context'ini yok et.
9. DirectX kaynaklarını bırak.
10. Hook'ları tamamen kaldır.
11. Gerekirse MinHook'u kapat.
12. Global uygulama işaretçisini en son temizle.

Bu sıra, herkes dışarı çıktıktan sonra ışıkları kapatıp kapıyı kilitlemeye
benzer.

## 18. Başlatma ve durdurma düğmeleri

DLL dışarıya dört önemli fonksiyon sunar.

### `StartOverlay()`

Overlay thread'ini başlatır.

Zaten çalışıyorsa ikinci bir kopya başlatmaz. Buna **idempotent davranış**
denir. Asansör çağırma düğmesine birkaç kez basınca beş farklı asansör
gelmemesi gibi düşünebilirsin.

### `RequestOverlayShutdown()`

“Lütfen güvenli biçimde kapanmaya başla” der. İşin bitmesini beklemez.

### `WaitForOverlayShutdown(...)`

Overlay thread'inin gerçekten bitmesini bekler.

### `IsOverlayRunning()`

Overlay thread'inin hâlâ çalışıp çalışmadığını sorar.

Doğru kapatma örneği:

```cpp
RequestOverlayShutdown();

if (WaitForOverlayShutdown(INFINITE) == WAIT_OBJECT_0) {
    FreeLibrary(overlayModule);
}
```

Satır satır anlamı:

1. Overlay'e kapanmasını söylüyoruz.
2. Thread tamamen bitene kadar bekliyoruz.
3. Gerçekten bittiyse DLL'i bellekten kaldırıyoruz.

`WaitForOverlayShutdown` başarısız olduysa veya zaman aşımına uğradıysa DLL
hemen kaldırılmamalıdır.

Bu fonksiyonların herkese açık tanımları
[`OverlayApi.hpp`](include/overlay/OverlayApi.hpp) dosyasındadır.

## 19. `DllMain` neden işi kendi başına başlatmıyor?

Windows bir DLL'i yüklerken `DllMain` adlı özel giriş noktasını çağırabilir.
Fakat bu sırada Windows'un yükleyici kilidi tutulabilir. Burada karmaşık işler
başlatmak kilitlenme ve yarış riskleri oluşturabilir.

Bu yüzden projedeki `DllMain` küçük tutulur:

- DLL'in adresini kaydeder,
- gereksiz thread bildirimlerini kapatır,
- ağır işleri başlatmaz.

Gerçek başlangıç, DLL yükleme işlemi tamamlandıktan sonra açıkça
`StartOverlay()` çağrısıyla yapılır.

## 20. WndProc nedir?

Windows pencereleri klavye, fare ve pencere boyutu gibi mesajlar alır.
Bu mesajları işleyen fonksiyona window procedure, kısaca **WndProc** denir.

Overlay menüsü açıkken ImGui'nin fare ve klavye mesajlarını görmesi gerekir.
Proje pencerenin WndProc akışına kontrollü biçimde katılır.

```text
Windows mesajı
      ↓
Overlay WndProcHook
      ↓
Menü kullanıyorsa ImGui işler
      ↓
Aksi hâlde orijinal WndProc'a gönderilir
```

Kapanırken orijinal WndProc mutlaka geri yüklenir. Ödünç alınan anahtarı sahibine
geri vermek gibi düşün.

## 21. Proje klasöründeki dosyalar

```text
DX11Overlay/
├── CMakeLists.txt
├── README.md
├── readme-1.md
├── BUILDING.md
├── LICENSE
├── include/
│   └── overlay/
├── src/
├── tests/
└── .github/
    └── workflows/
```

### `CMakeLists.txt`

Hangi kaynak dosyalarının derleneceğini, hangi kütüphanelerin kullanılacağını
ve çıktıların nasıl hazırlanacağını CMake'e anlatır.

Bir yemek tarifi listesine benzer.

### `include/overlay/`

Diğer dosyaların kullanabileceği başlıklar ve sözleşmeler burada bulunur.

- `Application.hpp`: Ana uygulama sınıfının tanımı.
- `Memory.hpp`: Güvenli bellek okuma fonksiyonlarının tanımı.
- `PatternScanner.hpp`: Tarayıcının tanımı.
- `Logger.hpp`: Log sisteminin tanımı.
- `OverlayApi.hpp`: DLL'in dışarı sunduğu başlatma ve kapatma API'si.

### `src/`

İşlerin gerçekten nasıl yapıldığını anlatan C++ kaynakları burada bulunur.

- `Application.cpp`: Hook, DX11, ImGui, menü ve yaşam döngüsü.
- `Memory.cpp`: Bellek doğrulama ve okuma.
- `PatternScanner.cpp`: Pattern ayrıştırma ve tarama.
- `Logger.cpp`: Log kayıtları.
- `DllMain.cpp`: DLL giriş noktası ve dış API.

### `tests/`

Kodun önemli davranışlarını otomatik kontrol eden küçük programlar vardır.

- `OverlayTests.cpp`: Bellek ve scanner kontrolleri.
- `LifecycleHost.cpp`: DLL başlatma, durdurma, bekleme ve yeniden başlatma
  kontrolleri.
- `Dx11SmokeHost.cpp`: Gerçek DX11 frame çizimi, overlay renderer kurulumu,
  pencere boyutlandırma ve temiz kapanış kontrolü.

### `.github/workflows/windows-ci.yml`

Proje GitHub'a gönderildiğinde Windows üzerinde Debug ve Release derlemelerini
otomatik çalıştırmak için hazırlanmış CI tarifidir.

## 22. Bu projeyi derlemek için ne gerekiyor?

Bu proje Windows x64 içindir.

Gerekenler:

1. Windows 10 veya Windows 11.
2. Visual Studio 2022.
3. Visual Studio içinde **Desktop development with C++** bileşeni.
4. CMake 3.24 veya daha yeni bir sürüm.
5. Varsayılan bağımlılık indirme yöntemi için Git ve internet erişimi.

Dear ImGui ve MinHook sürümleri CMake dosyasında sabitlenmiştir. Böylece bugün
çalışan projenin yarın rastgele değişen bir bağımlılık yüzünden farklı
davranması azaltılır.

## 23. Derleme komutları ne yapıyor?

Komutları **x64 Native Tools Command Prompt for VS 2022** veya Developer
PowerShell içinde, proje klasöründe çalıştırmalısın.

### Birinci adım: Hazırlık

```powershell
cmake -S . -B build -A x64 `
  -DDX11_OVERLAY_BUILD_TESTS=ON `
  -DDX11_OVERLAY_WARNINGS_AS_ERRORS=ON
```

Parçaların anlamı:

- `cmake`: CMake programını çalıştır.
- `-S .`: Kaynak kod şu an bulunduğumuz klasörde.
- `-B build`: Hazırlanan derleme dosyalarını `build` klasörüne koy.
- `-A x64`: 64 bit Windows için hazırla.
- `BUILD_TESTS=ON`: Test programlarını da hazırla.
- `WARNINGS_AS_ERRORS=ON`: Uyarıları ciddiye al ve hata gibi değerlendir.

### İkinci adım: Derleme

```powershell
cmake --build build --config Release --parallel
```

Anlamı:

- `build` klasöründeki tarifi kullan,
- optimize edilmiş `Release` sürümünü üret,
- mümkünse birden fazla işi aynı anda yap.

### Üçüncü adım: Test

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Anlamı:

- `build` içindeki Release testlerini çalıştır,
- hata olursa ayrıntılı çıktıyı göster.

### Dördüncü adım: Paketleme alanı

```powershell
cmake --install build --config Release --prefix stage
```

Anlamı:

- derlenen yayın dosyalarını,
- herkese açık API başlığını,
- lisansları,
- gerekli yardımcı dosyaları

`stage` klasöründe düzenli biçimde hazırla.

Tüm komutlar birlikte:

```powershell
cmake -S . -B build -A x64 `
  -DDX11_OVERLAY_BUILD_TESTS=ON `
  -DDX11_OVERLAY_WARNINGS_AS_ERRORS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix stage
```

Daha kısa ve teknik derleme belgesi için [`BUILDING.md`](BUILDING.md) dosyasına
bakabilirsin.

## 24. Testler neyi kontrol ediyor?

Test, öğretmenin cevap anahtarıyla ödev kontrol etmesine benzer.

### Bellek testleri

- Yerel bir tam sayı doğru okunabiliyor mu?
- Sıfır adresi reddediliyor mu?
- `PAGE_NOACCESS` bölgesi okunmadan reddediliyor mu?
- Yerel bir metin doğru okunabiliyor mu?
- Pointer chain beklenen son adrese ulaşıyor mu?

### Pattern testleri

- Geçerli pattern doğru ayrıştırılıyor mu?
- `?` ve `??` jokerleri çalışıyor mu?
- Hatalı pattern reddediliyor mu?
- Bilinen bir bayt dizisi bulunabiliyor mu?
- Önceden gönderilen durma isteğine uyuluyor mu?

### Yaşam döngüsü testleri

- DLL yüklenebiliyor mu?
- Dış API fonksiyonları bulunabiliyor mu?
- Overlay başlayabiliyor mu?
- İkinci başlatma isteği yeni bir kopya oluşturmadan başarılı oluyor mu?
- Sıfır süreli bekleme çalışan thread'i doğru görüyor mu?
- Kapanış isteği sonrası thread gerçekten bitiyor mu?
- Overlay temiz kapanıştan sonra yeniden başlayabiliyor mu?
- Başlangıç anında gelen kapanış isteği kayboluyor mu?

### DX11 smoke testi

- Gerçek bir DX11 pencere ve swap chain oluşturulabiliyor mu?
- `Present` hook çalışınca overlay renderer gerçekten kuruluyor mu?
- Hook aktifken `ResizeBuffers` başarıyla tamamlanıyor mu?
- Kapanışta orijinal WndProc geri yükleniyor mu?
- Overlay thread'i bittikten sonra DLL güvenle kaldırılabiliyor mu?

Testlerin geçmesi hiç hata olmadığı anlamına gelmez. Yalnızca yazılmış
kontrollerin başarılı olduğu anlamına gelir.

## 25. Sık karşılaşılabilecek sorunlar

### “DX11Overlay requires Windows” hatası

Proje Windows ve DirectX 11 için tasarlanmıştır. Linux üzerinde normal şekilde
derlenmez.

### CMake bulunamadı

CMake kurulmamış veya komut arama yoluna eklenmemiş olabilir.

### C++ derleyicisi bulunamadı

Visual Studio Installer içinden **Desktop development with C++** bileşeninin
kurulu olduğunu kontrol et.

### Bağımlılıklar indirilemiyor

İnternet veya Git erişimi olmayabilir. Dear ImGui ve MinHook varsayılan olarak
GitHub'dan indirilir.

### Menü görünmüyor

Şunları kontrol et:

- Hedef uygulama gerçekten DirectX 11 kullanıyor mu?
- `StartOverlay()` başarıyla çağrıldı mı?
- Uygulama uygun bir swap chain ile `Present` çağırıyor mu?
- `INSERT` tuşuyla menü kapatılmış olabilir mi?
- Log sekmesinde hata var mı?

### Pencere boyutlanınca sorun oluyor

`ResizeBuffersHook` ve yeni render target oluşturma adımlarını incele. Eski
render target'ın yeniden boyutlandırmadan önce bırakılması gerekir.

### DLL kapanmıyor

Önce `RequestOverlayShutdown()`, sonra `WaitForOverlayShutdown(...)`
çağrılmalıdır. Bekleme başarılı olmadan `FreeLibrary` kullanılmamalıdır.

## 26. Kod okumaya nereden başlamalıyım?

Hiç kod bilmiyorsan bütün dosyayı tek seferde anlamaya çalışma.

### Seviye 1: Haritayı öğren

1. Bu belgeyi baştan sona oku.
2. Klasör yapısını incele.
3. Dosya adlarının görevlerini tekrar et.

### Seviye 2: Dışarıdan görünen sözleşmeler

1. [`OverlayApi.hpp`](include/overlay/OverlayApi.hpp) dosyasını aç.
2. Dört dış fonksiyonun adını bul.
3. [`Memory.hpp`](include/overlay/Memory.hpp) dosyasındaki fonksiyon isimlerine
   bak.

İlk aşamada her sembolü anlaman gerekmiyor.

### Seviye 3: Küçük parçalar

1. [`Logger.cpp`](src/Logger.cpp) ile başla.
2. [`Memory.cpp`](src/Memory.cpp) içindeki okunabilirlik kontrollerini izle.
3. [`PatternScanner.cpp`](src/PatternScanner.cpp) içinde pattern parser'ı bul.

### Seviye 4: Büyük yaşam döngüsü

[`Application.cpp`](src/Application.cpp) içinde şu fonksiyonları sırayla bul:

1. `Initialize`
2. `InstallHooks`
3. `PresentHook`
4. `InitializeRenderer`
5. `ResizeBuffersHook`
6. `Render`
7. `Shutdown`

Bunları ayrı fonksiyonlar değil, tek bir hikâyenin bölümleri gibi oku.

### Seviye 5: Testlerle doğrula

[`OverlayTests.cpp`](tests/OverlayTests.cpp) ve
[`LifecycleHost.cpp`](tests/LifecycleHost.cpp) dosyalarında “Bu koddan ne
bekleniyor?” sorusunun cevaplarını ara.

## 27. Kod satırını nasıl okumalıyım?

Şu satırı ele alalım:

```cpp
m_running.store(false, std::memory_order_release);
```

İlk bakışta korkutucu görünebilir. Parçalara ayıralım:

- `m_running`: “uygulama çalışıyor mu?” bilgisini tutan kutu.
- `.store(...)`: kutuya yeni değer koy.
- `false`: artık çalışmıyor.
- `memory_order_release`: diğer thread'lerle görünürlük sırasını belirleyen
  ileri seviye kural.

Başlangıç seviyesinde satırı şöyle okuyabilirsin:

> “Çalışıyor bilgisini hayır olarak değiştir.”

Her ayrıntıyı ilk gün öğrenmek zorunda değilsin. Önce satırın amacını, sonra
mekanizmasını öğren.

Bir başka örnek:

```cpp
std::lock_guard renderLock(m_renderMutex);
```

Basit anlamı:

> “Bu blok bitene kadar render odasının anahtarını al.”

Blok bittiğinde `lock_guard` anahtarı otomatik geri bırakır. Bu da RAII
örneğidir.

## 28. Projenin bilinçli sınırları

Bu çalışma her olası durumu çözmeye çalışmaz.

- Yalnızca kendi prosesini okur.
- Birden fazla swap chain için gelişmiş seçim sistemi yoktur.
- Ekran kartı cihazının tamamen kaybolup yeniden oluşması, resize kadar ayrıntılı
  yönetilmez.
- Hook adresi bulma yöntemi DX11/DXGI düzeninin bilinen yapısına dayanır.
- Bir pattern bulunduğunda bunun beklenen fonksiyon olduğu otomatik kanıtlanmış
  olmaz.
- Windows dışındaki işletim sistemleri hedeflenmez.
- Yalnızca x64 yapı desteklenir.

Bu sınırlar utanılacak eksikler değildir. Projenin neyi çözmeye söz verdiğini
açıkça söyler.

## 29. Yeni başlayanlar için küçük alıştırmalar

Kod değiştirmeden yapılabilecekler:

1. Kâğıda başlangıçtan kapanışa kadar akış şeması çiz.
2. Hangi dosyanın hangi işi yaptığını kendi cümlelerinle yaz.
3. `Present`, `ResizeBuffers`, `StartOverlay` ve `Shutdown` kelimelerini kaynak
   dosyalarda bul.
4. Test dosyasındaki hata mesajlarını Türkçeye çevir.
5. Menüdeki sekmelerin hangi `Render...` fonksiyonuyla çizildiğini bul.

Küçük kod değişiklikleri:

1. Overlay başlığındaki metni değiştir.
2. İmleç çemberinin varsayılan yarıçapını değiştir.
3. Log ekranına farklı bir bilgi mesajı ekle.
4. Overview sekmesine sabit bir açıklama satırı ekle.
5. Bir tablo sütununun görünen başlığını değiştir.

Daha ileri çalışmalar:

1. Hex Viewer'a adres kopyalama düğmesi ekle.
2. Pattern sonuçlarını modül başlangıcına göre göreli adresle göster.
3. Büyük section'ları parça parça taramayı tasarla.
4. Birden fazla swap chain arasından seçim kuralı geliştir.
5. Renderer yaşam döngüsünü bir durum makinesi olarak çiz.

Her değişiklikten sonra:

1. Kodu derle.
2. Uyarıları oku.
3. Testleri çalıştır.
4. Ne değiştirdiğini küçük bir notla kaydet.

## 30. Bütün projeyi tek hikâyede özetleyelim

Bir tiyatro düşün:

- DX11 uygulaması sahnedeki oyundur.
- Her frame, oyunun o anki görüntüsüdür.
- Swap chain, sıradaki dekoru hazırlayıp sahneye çıkaran sistemdir.
- `Present`, “yeni sahneyi seyirciye göster” komutudur.
- Overlay, sahnenin önündeki şeffaf bilgi perdesidir.
- ImGui, bu perdeye buton ve yazıları çizen ressamdır.
- Hook, `Present` kapısında duran görevli gibidir.
- MinHook, görevlinin o kapıda güvenli biçimde durmasına yardım eder.
- Worker thread, arka odada arama yapan yardımcıdır.
- Mutex, aynı odaya iki kişinin aynı anda girip ortalığı karıştırmasını önleyen
  anahtardır.
- `ComPtr` ve RAII, işi biten araçların yerlerine konmasını sağlar.
- Shutdown sırası, tiyatro kapanırken önce çalışanları çıkarmak, sonra ışıkları
  kapatmak ve en son kapıyı kilitlemektir.

Tam akış:

```text
DLL yüklenir
    ↓
StartOverlay çağrılır
    ↓
Geçici DX11 deneme sahnesi kurulur
    ↓
Present ve ResizeBuffers adresleri bulunur
    ↓
Hook'lar kurulur
    ↓
İlk gerçek frame beklenir
    ↓
Gerçek pencere ve DX11 kaynakları alınır
    ↓
ImGui başlatılır
    ↓
Her frame'de overlay çizilir
    ↓
Gerekirse güvenli bellek okuma ve pattern tarama yapılır
    ↓
Kapanış isteği gelir
    ↓
Worker'lar durur, hook'lar kapanır, kaynaklar bırakılır
    ↓
Overlay thread'i tamamen biter
    ↓
DLL güvenle kaldırılabilir
```

## 31. Son söz

Bu projeyi anlamak için bütün C++ dilini bilmek zorunda değilsin.

Önce büyük resmi öğren:

1. Uygulama frame çizer.
2. Overlay doğru anda araya girer.
3. Kendi arayüzünü çizer.
4. Orijinal akışı devam ettirir.
5. Belleği okumadan önce izinleri kontrol eder.
6. Uzun işleri ayrı thread'de yapar.
7. Kapanırken her şeyi doğru sırayla bırakır.

Sonra küçük ayrıntıları sırayla öğrenebilirsin. İyi programlama, en karmaşık
satırı ezberlemek değil; sistemde kimin hangi işi yaptığını ve bir şey ters
gittiğinde nasıl güvenli kalacağını anlamaktır.

Teknik ve iki dilli mühendislik incelemesi için [`README.md`](README.md),
derleme özeti için [`BUILDING.md`](BUILDING.md), gerçekten çalıştırılmış
kontroller için [`VALIDATION.md`](VALIDATION.md), lisans için [`LICENSE`](LICENSE)
dosyasına bakabilirsin.

Bu proje [MIT Lisansı](LICENSE) altında dağıtılır.
