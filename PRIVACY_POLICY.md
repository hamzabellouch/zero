# Privacy Policy - Zero

Last Updated: October 17, 2026


### 0. Rule 0: Disclaimer on Modified Builds, External Binaries & Unofficial Forks:

- **Original Source Integrity:** This privacy policy, safety guarantees, and security features apply strictly and exclusively to the official, unmodified application binaries and source code distributed directly through this official repository ([hamzabellouch/zero](https://github.com/hamzabellouch/zero)) and its verified official releases.
- **Zero Liability for Modified / External Builds:** We (the publisher and maintainer) assume **no responsibility or liability whatsoever** for any modified binaries, third-party forks, repackaged installers, unofficial distribution channels, or altered executables not originating directly from this official repository. Any download, installation, or execution of modified, tampered, or third-party builds is entirely at your own risk, and the publisher assumes no responsibility for any consequences, system damages, security compromises, or data loss that may occur.


### 1. Executive Summary & Overview:

Zero is a fast, lightweight, and cross-platform desktop application designed to assist users with software uninstallation, residual file and registry cleaning, and real-time process monitoring.
#### Core Privacy Commitment:
Zero operates "100% on-device" and completely offline for application scanning, residual cleanup, and process monitoring. We do not collect, store, transmit, share, or sell any personal data, installed software inventories, running process metrics, device identifiers, or system telemetry to external analytics servers or third parties.


### 2. Information Processed On-Device:
To perform its core uninstallation, deep cleaning, and task management features, Zero reads and processes the following information `locally on your device`:

- **Installed Application Metadata:** Software display names, publisher details, versions, install locations, registry keys, and application icons are enumerated directly from local system registry hives and application directories in volatile memory.
- **Active System Process Metrics:** Process IDs (PIDs), executable names, executable paths, CPU utilization percentages, RAM working sets, disk I/O rates, and network bandwidth counters are sampled locally in real time via operating system diagnostic APIs.
- **File System & Registry Scanning Paths:** Candidate directories in local user directories (`AppData`, `ProgramData`, `Program Files`) and registry keys (`HKCU\Software`, `HKLM\Software`) are traversed locally to detect leftover residual fragments associated with target applications.
- **Local Preferences & UI State:** Minimal interface options (such as current view mode, sorting criteria, and window dimensions) are handled during runtime and never transmitted externally.


### 3. Permissions & System Access Used & Their Purposes:
Zero requests or requires specific operating system privileges strictly to deliver its core functionality:

**A. Administrator / Elevated Privileges** (`Run as Administrator` / `sudo`)
- **Purpose:** Enables Zero to launch third-party uninstaller executables that require elevation, delete leftover registry keys in `HKEY_LOCAL_MACHINE`, and safely remove residual files located in protected system directories (`Program Files`, `ProgramData`).
- **Scope:** Used strictly for executing user-initiated uninstallation tasks and deleting confirmed residual items.

**B. Process Query & Termination Rights** (`PROCESS_QUERY_INFORMATION`, `PROCESS_TERMINATE`)
- **Purpose:** Allows Zero's Process Engine to calculate real-time resource utilization (CPU, memory, disk I/O, network) and terminate unresponsive or unwanted process groups selected by the user.
- **Scope:** Used strictly within the Process Manager view for processes explicitly chosen by the user.

**C. File System & Registry Read/Write Access**
- **Purpose:** Required to discover installed desktop software, inspect residual traces, and delete selected leftover items.
- **Scope:** Restricted strictly to user application directories and registry nodes; protected operating system directories and critical hives are actively excluded.

**D. Zero Outbound Network Connectivity Policy**
- **Purpose:** Zero does not connect to the internet, contains no network sockets for communication, and performs no background telemetry updates.
- **Scope:** Operates 100% offline.


### 4. Cleaning & Process Termination Safety Policy:
Zero complies fully with system safety and data integrity guidelines:

- **Strict Operating System Isolation:** Critical operating system folders (`Windows`, `System32`, `Microsoft`, etc.) and essential system registry branches are protected with hardcoded safeguards and will never be scanned or deleted.
- **Sibling Application Protection:** Zero analyzes vendor keywords and tokens to distinguish sibling software (e.g., distinguishing Adobe Acrobat from Adobe Photoshop), preventing the accidental deletion of shared or related applications from the same publisher.
- **Recycle Bin Safe Recovery:** File system residuals are moved directly to the operating system's Recycle Bin (`FOF_ALLOWUNDO`) whenever possible, enabling full recovery in case of accidental removal.
- **Full User Consent & Control:** Leftover items are displayed in an interactive review dialog with clear checkboxes, giving users total control over what is kept or deleted before any action is executed.


### 5. Data Sharing, Analytics & Advertising:

- **No Data Sharing:** No application lists, process telemetry, or user data ever leaves your device.
- **No Third-Party Analytics / Tracking:** Zero does not include telemetry frameworks, crash analytics SDKs, or background tracking mechanisms.
- **No Ads:** Zero contains zero advertisements, promotional modules, or bundled third-party offers.


### 6. Data Retention & Lifecycle:

- **Volatile Memory Execution:** Process snapshots, resource counters, and leftover scan results are maintained only in volatile memory (RAM) and are discarded immediately upon dialog closure or application exit.
- **Zero Installation Footprint:** Zero is completely portable and standalone. It requires no background services, registers no system startup entries, and leaves zero persistent operational footprint on your system.


### 7. Managing Privileges & User Rights:
You maintain complete control over how Zero runs on your computer:

**A. Standard User vs. Administrator Execution:** You may run Zero as a standard non-elevated user to safely manage user-level applications and monitor processes without granting administrator privileges.

**B. Complete Portability:** Because Zero does not install background services or registry locks, deleting the standalone executable completely and instantly removes Zero from your device.


### 8. Contact & Support:
If you have any questions or feedback regarding this Privacy Policy or system access usage, please contact us at:
Email: hamzabellouchcontact@gmail.com


-------------------------------------------



# سياسة الخصوصية - Zero
آخر تحديث: ١٧ أكتوبر ٢٠٢٦

### ٠. القاعدة رقم ٠: إخلاء المسؤولية عن الإصدارات المعدلة والملفات التنفيذية الخارجية والمشاريع المشتقة:

* **سلامة المصدر الأصلي:** تنطبق سياسة الخصوصية، وضمانات الأمان، والميزات المذكورة في هذه الوثيقة حصرياً وصراحةً على الإصدارات الرسمية غير المعدلة من التطبيق والشفرة المصدرية المنشورة مباشرة عبر هذا المستودع الأصلي المعتمد ([hamzabellouch/zero](https://github.com/hamzabellouch/zero)) وصفحة إصداراته الرسمية فقط.
* **عدم تحمل المسؤولية عن النسخ المعدلة أو الخارجية:** نحن (الناشر والمطور) **لا نتحمل أي مسؤولية قانونية أو أمنية أو تقنية** عن تحميل أو تشغيل أي ملفات تنفيذية معدلة، أو حزم خارجية، أو نسخ مُعاد تجميعها، أو مشاريع مشتقة ومعدلة لا تنتمي لهذا المستودع الأصلي. كل ما ينتج عن استخدام أو تشغيل نسخ خارجية أو معدلة (من فقدان بيانات، أو أضرار بالنظام، أو ثغرات أمنية، أو برمجيات ضارة، أو أي تبعات ناجمة) يقع بالكامل على عاتق المستخدم وحده دون أدنى مسؤولية على الناشر.


### ١. الملخص التنفيذي والنظرة العامة:

تطبيق Zero هو أداة مكتبية سريعة وخفيفة ومتعددة المنصات، صُممت لمساعدة المستخدمين في إلغاء تثبيت البرامج، وتنظيف مخلفات الملفات وسجل النظام (الريجستري)، ومراقبة العمليات الجارية في الوقت الفعلي.
#### الالتزام الأساسي بالخصوصية:
يعمل Zero بنسبة "١٠٠٪ على الجهاز" وبشكل غير متصل بالإنترنت تماماً لجميع عمليات الفحص والتنظيف ومراقبة العمليات. نحن لا نجمع أو نخزن أو ننقل أو نشارك أو نبيع أي بيانات شخصية، أو قوائم بالبرامج المثبتة، أو قياسات موارد النظام، أو معرّفات الجهاز، أو سجلات الاستخدام لأي خوادم تحليلات خارجية أو أطراف ثالثة.


### ٢. المعلومات التي تتم معالجتها على الجهاز:
لتنفيذ ميزاته الأساسية الخاصة بإلغاء التثبيت والتنظيف العميق وإدارة المهام، يقوم Zero بقراءة ومعالجة المعلومات التالية "محليًا على جهازك":

* **بيانات البرامج المثبتة:** يتم استخراج أسماء البرامج، والناشر، والإصدارات، ومسارات التثبيت، ومفاتيح الريجستري، والأيقونات محليًا داخل الذاكرة المؤقتة (RAM) مباشرة من سجل النظام ومجلدات البرامج.
* **بيانات العمليات الجارية وموارد النظام:** يتم فحص معرّفات العمليات (PIDs)، وأسماء الملفات التنفيذية، ومساراتها، ونسب استهلاك المعالج (CPU)، والذاكرة (RAM)، ومعدلات القراءة والكتابة على القرص والشبكة محليًا في الوقت الفعلي عبر واجهات تشخيص النظام الرسمية.
* **مسارات فحص المخلفات وسجل النظام:** يتم فحص المجلدات المحلية للمستخدم (`AppData` و `ProgramData` و `Program Files`) ومفاتيح الريجستري (`Software`) محلياً لرصد الملفات والمفاتيح المتبقية التابعة للتطبيقات المستهدفة.
* **التفضيلات وحالة واجهة المستخدم:** يتم التعامل مع تفضيلات العرض البسيطة وأبعاد النوافذ أثناء وقت التشغيل فقط ولا يتم إرسالها خارج الجهاز أبداً.


### ٣. الصلاحيات والوصول للنظام وأغراضها:
يطلب Zero صلاحيات وصول محددة لنظام التشغيل حصرياً لتقديم وظائفه الأساسية:

**أ. صلاحيات المسؤول / الصلاحيات المرتفعة** (`Run as Administrator` / `sudo`)
* **الغرض:** تمكين Zero من تشغيل برامج إلغاء التثبيت التي تتطلب إذناً إدارياً، وحذف مفاتيح الريجستري المتبقية في `HKEY_LOCAL_MACHINE`، وإزالة المخلفات من المجلدات المحمية (`Program Files` و `ProgramData`).
* **النطاق:** يُستخدم حصريًا لتنفيذ عمليات إلغاء التثبيت وحذف المخلفات المؤكدة بطلب صريح من المستخدم.

**ب. حقوق فحص وإنهاء العمليات** (`PROCESS_QUERY_INFORMATION` و `PROCESS_TERMINATE`)
* **الغرض:** يسمح لمحرك العمليات في Zero بحساب معدلات استهلاك الموارد وإنهاء مهام العمليات المحددة أو المتجمدة.
* **النطاق:** يُستخدم حصرياً داخل شاشة إدارة العمليات للعمليات التي يختارها المستخدم بنفسه.

**ج. الوصول لنظام الملفات وسجل النظام (الريجستري)**
* **الغرض:** ضروري لاكتشاف البرامج المثبتة وفحص الآثار المتبقية وحذف عناصر المخلفات المختارة.
* **النطاق:** مقتصر بشكل صارم على مجلدات التطبيقات ومفاتيح الريجستري المرتبطة بها؛ مع استبعاد ملفات ومجلدات نظام التشغيل المحمية تلقائياً.

**د. سياسة انعدام الاتصال بالشبكة (Zero Network Connectivity)**
* **الغرض:** لا يتصل Zero بشبكة الإنترنت، ولا يحتوي على بروتوكولات اتصال أو برمجيات إرسال قياسات تتبع خارجية.
* **النطاق:** يعمل بشكل غير متصل بالإنترنت بنسبة ١٠٠٪.


### ٤. سياسة أمان التنظيف وإنهاء العمليات:
يلتزم Zero التزاماً صارماً بمعايير سلامة النظام وحماية البيانات:

* **عزل كامل لملفات ومجلدات النظام:** المجلدات الحيوية لنظام التشغيل (`Windows` و `System32` و `Microsoft` وغيرها) وفروع الريجستري الأساسية محمية بحواجز برمجية صارمة ولا يتم فحصها أو حذفها مطلقاً.
* **حماية التطبيقات الشقيقة (Sibling Application Protection):** يقوم محرك فحص الذكاء الداخلي بتحليل أسماء الشركات المطورة والبرامج ذات الصلة (مثل التمييز بين Adobe Acrobat و Adobe Photoshop) لمنع حذف أي مكتبات أو ملفات مشتركة تابعة لبرامج شقيقة أخرى لنفس الشركة.
* **الحذف الآمن عبر سلة المحذوفات (Recycle Bin):** يتم إرسال الملفات والمجلدات المتبقية التي يتقرر حذفها إلى سلة محذوفات النظام (`FOF_ALLOWUNDO`) لتمكين المستخدم من استرجاعها في حال الحذف بالخطأ.
* **موافقة المستخدم والتحكم الكامل:** تُعرض جميع عناصر المخلفات المكتشفة في نافذة مراجعة تفاعلية مع خانات اختيار واضحة، مما يمنح المستخدم السيطرة الكاملة على ما يريد حذفه أو الإبقاء عليه قبل التنفيذ.


### ٥. مشاركة البيانات والتحليلات والإعلانات:

* **عدم مشاركة البيانات:** لا تغادر أي بيانات عن البرامج أو العمليات أو الجهاز حاسوبك الشخصي.
* **عدم وجود تحليلات أو تتبع من جهات خارجية:** لا يدمج Zero أي حزم برمجية للتحليلات أو رصد الأعطال أو التتبع الإحصائي.
* **لا توجد إعلانات:** لا يحتوي Zero على أي إعلانات تجارية أو عروض مدمجة.


### ٦. الاحتفاظ بالبيانات ودورة حياتها:

* **معالجة مؤقتة في الذاكرة (RAM):** يتم الاحتفاظ بنتائج فحص العمليات والمخلفات داخل الذاكرة العشوائية المؤقتة فقط، ويتم التخلص منها فور إغلاق النوافذ أو إنهاء التطبيق.
* **انعدام الأثر المحلي (Zero Residual Footprint):** تطبيق Zero محمول بالكامل، ولا يقوم بتثبيت خدمات في الخلفية، ولا يضيف قيوداً على بدء التشغيل، ولا يترك ملفات إعدادات غير مرغوبة في النظام.


### ٧. إدارة الصلاحيات وحقوق المستخدم:
تحتفظ بالتحكم الكامل في كيفية تشغيل Zero على جهازك:

**أ. التشغيل كمستخدم عادي مقابل التشغيل كمسؤول:** يمكنك تشغيل Zero كمستخدم عادي دون صلاحيات مسؤول لإلغاء تثبيت البرامج الخاصة بالمستخدم ومراقبة العمليات بأمان تام.

**ب. المحمولية الكاملة:** نظراً لأن Zero لا يثبت أي خدمات في النظام، فإن حذف الملف التنفيذي المحمول يؤدي فوراً إلى إزالة الأداة من جهازك بالكامل دون أي بقايا.


### ٨. التواصل والدعم:
إذا كانت لديك أي أسئلة أو ملاحظات بشأن سياسة الخصوصية هذه أو استخدام الصلاحيات، فيرجى التواصل معنا على:
البريد الإلكتروني: hamzabellouchcontact@gmail.com
