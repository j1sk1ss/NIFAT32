use eframe::egui::{
    self, Color32, FontId, Pos2, Rect, Sense, Slider, Ui, Vec2, Align2
};

struct FsApp {
    disk_size_mb: u32,
    fat_count: u32,
    spc: u32,
    boot_sector_size: u32,
    boot_sector_count: u32,
    journals_count: u32,
    file_metadata_size: u32,
    encoding_coef: u32,
    bytes_per_sector: u32,
    zoom: f32
}

impl Default for FsApp {
    fn default() -> FsApp {
        return FsApp {
            disk_size_mb: 10,
            fat_count: 2,
            spc: 8,
            boot_sector_size: 81,
            boot_sector_count: 20,
            journals_count: 0,
            file_metadata_size: 28,
            encoding_coef: 2,
            bytes_per_sector: 512,
            zoom: 1.0
        };
    }
}

struct Layout {
    boot: u32,
    journals: u32,
    fat: u32,
    metadata: u32,
    file_data: u32,
    total_sectors: u32,
    efficiency: f32,
    max_file_count: u32
}

fn compute_layout(app: &FsApp) -> Layout {
    let total_sectors = (app.disk_size_mb * 1024 * 1024) / app.bytes_per_sector;
    let boot_sectors_total = (app.boot_sector_count * app.boot_sector_size * app.encoding_coef) / app.bytes_per_sector;
    let journal_sectors_total = app.journals_count * app.spc;

    let mut fat_size = 0;
    let mut prev_fat_size = u32::MAX;

    while fat_size != prev_fat_size {
        prev_fat_size = fat_size;
        let data_sectors = total_sectors - app.fat_count * fat_size * app.encoding_coef - boot_sectors_total - journal_sectors_total;
        let clusters = data_sectors / app.spc;
        fat_size = ((clusters * 4 + app.bytes_per_sector - 1) / app.bytes_per_sector) as u32;
    }

    let fat_total = fat_size * app.fat_count;
    let mut data_sectors = total_sectors - boot_sectors_total - fat_total - journal_sectors_total;
    let cluster_size_bytes = app.spc * app.bytes_per_sector;
    let total_clusters = data_sectors / app.spc;
    let metadata_per_cluster = cluster_size_bytes / (app.file_metadata_size * app.encoding_coef);

    let max_file_count = {
        let (mut low, mut high) = (0, total_clusters);
        while low < high {
            let mid = (low + high + 1) / 2;
            let overhead = (mid + metadata_per_cluster - 1) / metadata_per_cluster;
            if mid + overhead <= total_clusters {
                low = mid;
            } else {
                high = mid - 1;
            }
        }
        
        low
    };

    let metadata_clusters = (max_file_count + metadata_per_cluster - 1) / metadata_per_cluster;
    data_sectors -= metadata_clusters * app.spc;

    return Layout {
        boot: boot_sectors_total,
        journals: journal_sectors_total,
        fat: fat_total,
        metadata: metadata_clusters * app.spc,
        file_data: data_sectors,
        total_sectors,
        efficiency: (data_sectors as f32 / total_sectors as f32) * 100.0,
        max_file_count,
    };
}

fn draw_layout(ui: &mut Ui, layout: &Layout, zoom: f32) {
    let total = layout.total_sectors as f32;
    let segments = [
        (layout.boot, "Boot", Color32::from_rgb(32, 32, 32)),
        (layout.journals, "Journals", Color32::from_rgb(80, 80, 80)),
        (layout.fat, "FAT", Color32::from_rgb(112, 112, 112)),
        (layout.metadata, "Metadata", Color32::from_rgb(160, 160, 255)),
        (layout.file_data, "File Data", Color32::from_rgb(220, 220, 220)),
    ];

    let base_width = ui.available_width() * zoom;
    let (rect, _) = ui.allocate_exact_size(Vec2::new(base_width, 60.0), Sense::hover());
    let painter = ui.painter();

    let mut x = rect.left();
    for (size, label, color) in segments.iter() {
        let width = (*size as f32 / total) * rect.width();
        let seg_rect = Rect::from_min_size(Pos2::new(x, rect.top()), Vec2::new(width, rect.height()));
        painter.rect_filled(seg_rect, 0.0, *color);
        if *size as f32 / total > 0.02 {
            painter.text(
                seg_rect.center(),
                Align2::CENTER_CENTER,
                format!("{}\n{} sectors", label, size),
                FontId::monospace(10.0),
                Color32::BLACK,
            );
        }

        let response = ui.interact(seg_rect, ui.id().with(*label), Sense::hover());
        if response.hovered() {
            let percent = (*size as f32 / total) * 100.0;
            response.on_hover_text(format!(
                "{}: {} sectors ({:.2}%)",
                label, size, percent
            ));
        }

        x += width;
    }

    ui.label(format!(
        "Max files count: {}, Usable space: {:.2}%",
        layout.max_file_count, layout.efficiency
    ));
}

impl eframe::App for FsApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) -> () {
        egui::SidePanel::left("controls").show(ctx, |ui| {
            ui.heading("NIFAT32 params");
            ui.add(Slider::new(&mut self.disk_size_mb, 1..=20480).text("Image size (MB)"));
            ui.add(Slider::new(&mut self.bytes_per_sector, 64..=1024).text("Bytes per sector").step_by(64.0));
            ui.add(Slider::new(&mut self.spc, 1..=64).text("Sectors per cluster"));
            ui.add(Slider::new(&mut self.boot_sector_size, 1..=512).text("Bootsector size"));
            ui.add(Slider::new(&mut self.boot_sector_count, 1..=100).text("Bootsectors count"));
            ui.add(Slider::new(&mut self.journals_count, 0..=100).text("Journals count"));
            ui.add(Slider::new(&mut self.fat_count, 1..=8).text("FAT Count"));
            ui.add(Slider::new(&mut self.file_metadata_size, 1..=128).text("directory_entry_t size"));
            ui.add(Slider::new(&mut self.encoding_coef, 1..=4).text("Encoding coef"));
        });

        egui::CentralPanel::default().show(ctx, |ui| {
            let layout = compute_layout(self);
            draw_layout(ui, &layout, self.zoom);
            ui.add(egui::Slider::new(&mut self.zoom, 0.1..=5.0).text("Zoom X"));
        });
    }
}

fn main() -> Result<(), eframe::Error> {
    let options = eframe::NativeOptions::default();
    eframe::run_native(
        "FAT32 Layout Viewer",
        options,
        Box::new(|_cc| Box::new(FsApp::default())),
    )
}
