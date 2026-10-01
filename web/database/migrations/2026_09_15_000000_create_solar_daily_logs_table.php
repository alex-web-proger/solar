<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('solar_daily_logs', function (Blueprint $table) {
            $table->id();

            // Сутки, за которые ведётся запись. Одна запись = одни сутки.
            $table->date('date')->unique();

            // Выработка по каждому из 4 MPPT-контроллеров Victron, Втч
            $table->decimal('mppt1_wh', 8, 1)->default(0);
            $table->decimal('mppt2_wh', 8, 1)->default(0);
            $table->decimal('mppt3_wh', 8, 1)->default(0);
            $table->decimal('mppt4_wh', 8, 1)->default(0);

            // Суммарная выработка по всем контроллерам за сутки, Втч
            $table->decimal('total_generated_wh', 8, 1)->default(0);

            // Потреблённая мощность за сутки, Втч
            $table->decimal('consumed_wh', 8, 1)->default(0);

            // Текущий заряд аккумулятора на момент последнего обновления записи, %
            $table->decimal('battery_soc', 6, 2)->nullable();

            $table->timestamps();
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('solar_daily_logs');
    }
};
